import os
import json
import base64
import re
import traceback
import urllib.request
import importlib.util
import inspect
from pathlib import Path
from tool import Tool
from prompt import Prompt
import trafilatura

# ###############################
# Globals
# ###############################

CLUSTER = os.environ.get('CLUSTER')
ENVIRONMENT_VERSION = os.environ.get('ENVIRONMENT_VERSION')

INVALID_REQUEST_CODE    = -32600
METHOD_NOT_FOUND_CODE   = -32601
INVALID_PARAMS_CODE     = -32602
INTERNAL_ERROR_CODE     = -32603
PARSE_ERROR_CODE        = -32700

SUPPORTED_PROTOCOL_VERSIONS = ["2024-11-05", "2025-03-26", "2025-06-18"]
LATEST_PROTOCOL_VERSION     = SUPPORTED_PROTOCOL_VERSIONS[-1]

# ###############################
# Utilities
# ###############################

#
# Custom parsing code for API Gateway arrays
#
def parse_claim_array(claim_value):
    if isinstance(claim_value, str):
        if claim_value.startswith('['):
            return claim_value.strip('[]').split() # Remove brackets and split by whitespace
        else:
            return [claim_value]
    else:
        return []

#
# Handle encoded body
#
def get_body(event):
    body_raw = event.get("body")
    if body_raw:
        if event.get("isBase64Encoded"):
            body_raw = base64.b64decode(body_raw).decode("utf-8")
        return json.loads(body_raw)
    else:
        return {}

#
# API Gateway Response Format
#
def gateway_response(status_code, body):
    return {
        'statusCode': status_code,
        'headers': {
            'Content-Type': 'application/json',
        },
        'body': json.dumps(body)
    }

#
# Error Handling
#
def rpc_error(rqst, error_code, message):
    print(f'Error: {message}')
    return {
        "jsonrpc": "2.0",
        "id": rqst["id"],
        "error": {
            "code": error_code,
            "message": message
        }
    }

#
# Result Handling
#
def rpc_result(rqst, result):
    return {
        "jsonrpc": "2.0",
        "id": rqst["id"],
        "result": result
    }

#
# Parse Request
#
def parse_request(event):
    # pull out claims
    claims = event["requestContext"]["authorizer"]["jwt"]["claims"] # get JWT claims (validated by API Gateway)
    username = claims.get('sub', '<anonymous>')
    org_roles = parse_claim_array(claims.get('org_roles', "[]"))
    role = "owner" in org_roles and "owner" or "member" in org_roles and "member" or "affiliate" in org_roles and "affiliate" or "guest"
    # pull out request parameters
    path = event.get("rawPath", '')
    body = get_body(event)
    method = body["method"]
    parms = body.get("params", {})
    rqst_id = body.get("id") # absent id indicates a JSON-RPC notification
    # display diagnostic message
    print(f'Received request to {path} from {username} ({role}): {method} - {parms}') # diagnostic
    # build and return request info
    return {
        "path": path,
        "username": username,
        "role": role,
        "method": method,
        "parms": parms,
        "id": rqst_id
    }

#
# Validate Request
#
def validate_request(rqst):
    # check organization membership (only required when calling a tool)
    if (rqst["method"] == "tools/call") and (rqst["role"] not in ["owner", "member", "affiliate"]):
        return INVALID_REQUEST_CODE, f'access to tools denied to {rqst["username"]}, organization role: {rqst["role"]}'
    # success
    return 0, None

#
# Get Available Prompts
#
def get_available_prompts():
    prompts = {}
    for py_file in Path("prompts").glob("*.py"):
        module_name = py_file.stem
        spec = importlib.util.spec_from_file_location(module_name, py_file)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for _class_name, prompt_class in inspect.getmembers(module, inspect.isclass):
            if prompt_class.__module__ == module_name and issubclass(prompt_class, Prompt):
                prompt = prompt_class()
                prompts[prompt.name] = prompt
    return prompts

#
# Get Available Resources
#
def get_available_resources():
    with open("resources/resources.json") as file:
        return json.load(file)

#
# Get Available Templates
#
def get_available_templates():
    with open("resources/templates.json") as file:
        return json.load(file)

#
# Search and Load Available Tools
#
def get_available_tools():
    tools = {}
    for py_file in Path("tools").glob("*.py"):
        module_name = py_file.stem
        spec = importlib.util.spec_from_file_location(module_name, py_file)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for _class_name, tool_class in inspect.getmembers(module, inspect.isclass):
            if tool_class.__module__ == module_name and issubclass(tool_class, Tool):
                tool = tool_class()
                tools[tool.name] = tool
    return tools

#
# Convert Jupyter Notebook to Markdown
#
def notebook_to_markdown(ipynb):
    notebook = json.loads(ipynb)
    markdown = []
    def cell_source(cell):
        source = cell.get("source", "")
        if isinstance(source, list):
            return "".join(source)
        return source
    for cell in notebook.get("cells", []):
        source = cell_source(cell).strip()
        if not source:
            continue
        if cell.get("cell_type") == "markdown":
            markdown.append(source)
        elif cell.get("cell_type") == "code":
            fence = "````" if "```" in source else "```"
            markdown.append(f"{fence}python\n{source}\n{fence}")
    return "\n\n".join(markdown)

# ###############################
# Cached Globals
# ###############################

TOOLS = get_available_tools()
RESOURCES = get_available_resources()
TEMPLATES = get_available_templates()
PROMPTS = get_available_prompts()

# ###############################
# Method Handlers
# ###############################

#
# Initialize
#
def initialize_handler(rqst):
    # honor the client's requested protocol version if we support it, otherwise
    # respond with our latest supported version (per the MCP initialization spec)
    requested_version = rqst["parms"].get("protocolVersion")
    protocol_version = requested_version if requested_version in SUPPORTED_PROTOCOL_VERSIONS else LATEST_PROTOCOL_VERSION
    return rpc_result(rqst, {
        "protocolVersion": protocol_version,
        "capabilities": {
            "tools": {},
            "resources": {},
            "prompts": {},
        },
        "serverInfo": {
            "name": "SlideRule MCP Server",
            "version": f"{ENVIRONMENT_VERSION}"
        }
    })

#
# Ping
#
def ping_handler(rqst):
    return rpc_result(rqst, {})

#
# List Tools
#
def tools_list_handler(rqst):
    return rpc_result(rqst, {
        "tools": [tool.definition for tool in TOOLS.values()]
    })

#
# Call Tool
#
def tools_call_handler(rqst):
    name = rqst["parms"]["name"]
    if name not in TOOLS:
        return rpc_error(rqst, INVALID_PARAMS_CODE, f'tool {name} not found')
    arguments = rqst["parms"].get("arguments", {})
    content = TOOLS[name].call(arguments)
    if content is None:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'tool {name} is not implemented')
    return rpc_result(rqst, {
        "content": content if isinstance(content, list) else [content]
    })

#
# List Resources
#
def resources_list_handler(rqst):
    return rpc_result(rqst, {
        "resources": RESOURCES
    })

#
# Template Resources
#
def resources_template_handler(rqst):
    return rpc_result(rqst, {
        "resourceTemplates": TEMPLATES
    })

#
# Read Resource
#
def resources_read_handler(rqst):

    # get request resource uri
    uri = rqst["parms"]["uri"]

    # get matched resource
    available_resources = RESOURCES + TEMPLATES
    matched_resource = None
    for resource in available_resources:
        if "uri" in resource:
            if uri == resource["uri"]:
                matched_resource = resource
                break
        elif "uriTemplate" in resource:
            pattern = re.sub(r'\\\{(\w+)\\\}', r'(?P<\1>[^/]+)', re.escape(resource["uriTemplate"]))
            pattern = re.compile(f'^{pattern}$')
            if pattern.match(uri):
                matched_resource = resource
                break

    # check matched resource
    if not matched_resource:
        raise RuntimeError("unable to locate resource")

    # get resource path
    resource_path = uri.split("://")[-1].split("/")

    # mcp dataset resource
    if resource_path[0] == "mcp" and resource_path[1] == "datasets":
        mcp_id = resource_path[-1]
        text = json.dumps(Tool.datasetStatus(mcp_id))

    # mcp job resource
    elif resource_path[0] == "mcp" and resource_path[1] == "jobs":
        mcp_id = resource_path[-1]
        text = json.dumps(Tool.jobStatus(mcp_id))

    # mcp raw resource
    elif resource_path[0] == "mcp":
        with open(f"resources/{'/'.join(resource_path[1:])}") as file:
            text = file.read()

    # openapi resource
    elif resource_path[0] == "openapi":
        with urllib.request.urlopen(f"https://sliderule.slideruleearth.io/openapi/{'/'.join(resource_path[1:])}") as rsp:
            text = rsp.read().decode("utf-8")

    # doc/python resource
    elif resource_path[0] == "docs" and resource_path[1] == "python":
        with urllib.request.urlopen(f"https://docs.slideruleearth.io/{'/'.join(resource_path[2:])}.html") as resp:
            html = resp.read().decode("utf-8")
        text = trafilatura.extract(html, output_format="markdown")

    # example/python resource
    elif resource_path[0] == "examples" and resource_path[1] == "python":
        with urllib.request.urlopen(f"https://docs.slideruleearth.io/_static/{'/'.join(resource_path[2:])}.ipynb") as rsp:
            ipynb = rsp.read().decode("utf-8")
        text = notebook_to_markdown(ipynb)

    # unhandled resource
    else:
        raise RuntimeError("failed to parse resource path")

    # return resource content
    return rpc_result(rqst, {
        "contents": [ {
            "uri": uri,
            "mimeType": matched_resource["mimeType"],
            "title": matched_resource["name"],
            "text": text
        } ]
    })

#
# List Prompts
#
def prompts_list_handler(rqst):
    return rpc_result(rqst, {
        "prompts": [prompt.definition for prompt in PROMPTS.values()]
    })

#
# Get Prompts
#
def prompts_get_handler(rqst):
    name = rqst["parms"]["name"]
    if name not in PROMPTS:
        return rpc_error(rqst, INVALID_PARAMS_CODE, f'prompt {name} not found')
    arguments = rqst["parms"].get("arguments", {})
    return rpc_result(rqst, {
        "messages": PROMPTS[name].respond(arguments)
    })

# ###############################
# JSON RPC Processing
# ###############################

#
# Method Routing
#
METHODS = {
    "initialize":               initialize_handler,
    "ping":                     ping_handler,
    "tools/list":               tools_list_handler,
    "tools/call":               tools_call_handler,
    "resources/list":           resources_list_handler,
    "resources/templates/list": resources_template_handler,
    "resources/read":           resources_read_handler,
    "prompts/list":             prompts_list_handler,
    "prompts/get":              prompts_get_handler,
}

#
# Method Invocation
#
def rpc_response(rqst):
    try:
        if rqst["method"] in METHODS:
            return METHODS[rqst["method"]](rqst)
        else:
            return rpc_error(rqst, METHOD_NOT_FOUND_CODE, f'method {rqst["method"]} not found')
    except Exception as e:
        traceback.print_exc()
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

# ###############################
# Lambda Gateway
# ###############################

def lambda_gateway(event, context):
    """
    Lambda entry point for API Gateway
    """
    try:
        # process request
        rqst = parse_request(event)

        # handle notifications (no id) - no response body is expected
        if rqst["id"] is None:
            return gateway_response(202, {})

        code, msg = validate_request(rqst)

        # route request
        if   code < 0:                              return gateway_response(200, rpc_error(rqst, code, msg))
        elif rqst["path"] == f'/{CLUSTER}/info':    return gateway_response(200, {"environment_version": ENVIRONMENT_VERSION})
        elif rqst["path"] == f'/{CLUSTER}':         return gateway_response(200, rpc_response(rqst))
        else:                                       return gateway_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return gateway_response(500, {'error': 'unhandled exception', 'exception': f'{e}'})
