import os
import json
import base64
import boto3
import sandbox
import applications.mcp.resource as resource

# ###############################
# Cached Objects
# ###############################

s3 = boto3.client("s3")


# ###############################
# Globals
# ###############################

CLUSTER = os.environ.get('CLUSTER')
PROJECT_BUCKET = os.environ.get("PROJECT_BUCKET")
PROJECT_PUBLIC_BUCKET = os.environ.get("PROJECT_PUBLIC_BUCKET")
ENVIRONMENT_VERSION = os.environ.get('ENVIRONMENT_VERSION')

INVALID_REQUEST_CODE    = -32600
METHOD_NOT_FOUND_CODE   = -32601
INVALID_PARAMS_CODE     = -32602
INTERNAL_ERROR_CODE     = -32603
PARSE_ERROR_CODE        = -32700


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
    parms = body["params"]
    rqst_id = body["id"]

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

# ###############################
# Method Handlers
# ###############################

#
# Initialize
#
def initialize_handler(rqst):
    return rpc_result(rqst, {
        "protocolVersion": "2025-03-26",
        "capabilities": {
            "tools": {},
            "resources": {},
            "prompts": {},
        },
        "serverInfo": {
            "name": f"SlideRule MCP Server",
            "version": f"{ENVIRONMENT_VERSION}"
        }
    })

#
# List Tools
#
def tools_list_handler(rqst):
    return rpc_result(rqst, {})

#
# Call Tool
#
def tools_call_handler(rqst):
    return rpc_result(rqst, {})

#
# List Resources
#
def resources_list_handler(rqst):
    return rpc_result(rqst, {
        "resources": resource.resources()
    })

#
# Template Resources
#
def resources_template_handler(rqst):
    return rpc_result(rqst, {
        "resourceTemplates": resource.templates()
    })

#
# Read Resource
#
def resources_read_handler(rqst):
    return rpc_result(rqst, {
        "contents": [ resource.read(rqst["parms"]["uri"]) ]
    })

#
# List Prompts
#
def prompts_list_handler(rqst):
    with open("prompts/prompts.json") as file:
        return rpc_result(rqst, {
            "prompts": json.load(file)
        })

#
# Get Prompts
#
def prompts_get_handler(rqst):
    return rpc_result(rqst, {})

# ###############################
# JSON RPC Processing
# ###############################

#
# Method Routing
#
METHODS = {
    "initialize":               initialize_handler,
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
        code, msg = validate_request(rqst)

        # route request
        if   code < 0:                              return gateway_response(200, rpc_error(rqst, code, msg))
        elif rqst["path"] == f'/{CLUSTER}/info':    return gateway_response(200, {"environment_version": ENVIRONMENT_VERSION})
        elif rqst["path"] == f'/{CLUSTER}':         return gateway_response(200, rpc_response(rqst))
        else:                                       return gateway_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return gateway_response(500, {'error': 'unhandled exception', 'exception': f'{e}'})
