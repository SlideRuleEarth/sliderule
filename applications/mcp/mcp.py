import os
import json
import base64
import boto3
import sandbox

# ###############################
# Cached Objects
# ###############################

s3 = boto3.client("s3")

# ###############################
# Globals
# ###############################

STACK_NAME = os.environ.get("STACK_NAME")
DOMAIN = os.environ.get("DOMAIN")
CLUSTER = os.environ.get('CLUSTER')
MCP_HOSTNAME = os.environ.get("MCP_HOSTNAME")
PROJECT_BUCKET = os.environ.get("PROJECT_BUCKET")
PROJECT_PUBLIC_BUCKET = os.environ.get("PROJECT_PUBLIC_BUCKET")
JWT_ISSUER = os.environ.get('JWT_ISSUER')
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
# Validate Request
#
def validate_request(event):

    # pull out claims
    claims = event["requestContext"]["authorizer"]["jwt"]["claims"] # get JWT claims (validated by API Gateway)
    username = claims.get('sub', '<anonymous>')
    org_roles = parse_claim_array(claims.get('org_roles', "[]"))

    # pull out request parameters
    path = event.get("rawPath", '')
    body = get_body(event)
    method = body["method"]
    parms = body["params"]
    rqst_id = body["id"]

    # check organization membership (only required when calling a tool)
    role = "owner" in org_roles and "owner" or "member" in org_roles and "member" or "affiliate" in org_roles and "affiliate" or "guest"
    if (method == "tools/call") and (role not in ["owner", "member", "affiliate"]):
        print(f'Access to tools denied to {username}, organization roles: {org_roles}')
        return None

    # build and return request info
    return {
        "path": path,
        "username": username,
        "role": role,
        "id": rqst_id,
        "method": method,
        "parms": parms
    }

# ###############################
# Method Handlers
# ###############################

#
# Initialize
#
def initialize_handler(rqst):
    try:
        return rpc_result(rqst, {})
    except Exception as e:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

#
# List Tools
#
def tools_list_handler(rqst):
    try:
        return rpc_result(rqst, {})
    except Exception as e:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

#
# Call Tool
#
def tools_call_handler(rqst):
    try:
        return rpc_result(rqst, {})
    except Exception as e:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

#
# List Resources
#
def resources_list_handler(rqst):
    try:
        return rpc_result(rqst, {})
    except Exception as e:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

#
# Template Resources
#
def resources_template_handler(rqst):
    try:
        return rpc_result(rqst, {})
    except Exception as e:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

#
# Read Resource
#
def resources_read_handler(rqst):
    try:
        return rpc_result(rqst, {})
    except Exception as e:
        return rpc_error(rqst, INTERNAL_ERROR_CODE, f'{e}')

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
    "resources/list":           resources_template_handler,
    "resources/templates/list": resources_list_handler,
    "resources/read":           resources_read_handler
}

#
# Method Invocation
#
def rpc_response(rqst):
    try:
        if rqst["method"] in METHODS:
            return METHODS[rqst["method"]](rqst["params"])
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
        rqst = validate_request(event) # validate request against claims and return safe request parameters
        print(f'Received request to {rqst["path"]} from {rqst["username"]} ({rqst["role"]}): {rqst["method"]} - {rqst["params"]}') # diagnostic

        # route request
        if   rqst == None:                              return gateway_response(200, rpc_error(rqst, INVALID_REQUEST_CODE, f'invalid request'))
        elif rqst["path"] == f'/{CLUSTER}/info':        return gateway_response(200, {"environment_version": ENVIRONMENT_VERSION})
        elif rqst["path"] == f'/{CLUSTER}':             return gateway_response(200, rpc_response(rqst))
        else:                                           return gateway_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return gateway_response(500, {'error': 'unhandled exception', 'exception': f'{e}'})

# ###############################
# Main: Local Test Environment
# ###############################

if __name__ == '__main__':

    # imports
    import sliderule
    import argparse

    # command line arguments
    parser = argparse.ArgumentParser(description="""Provisioner Command Line""")
    parser.add_argument('--tool',       type=str,               default=None)
    parser.add_argument('--verbose',    action='store_true',    default=False)
    args = parser.parse_args()

    # sliderule python client session
    session = sliderule.create_session(domain=DOMAIN, verbose=args.verbose)
    session.authenticate()

    # build request
    rqst = {
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": f'[{" ".join(session.ps_metadata["org_roles"])}]',
                        "aud": f'[{" ".join(session.ps_metadata["aud"])}]',
                        "sub": f'{session.ps_metadata["sub"]}'
                    }
                }
            }
        },
        "headers": {
            "host": DOMAIN
        },
        "rawPath": args.tool,
        "body": json.dumps({})
    }

    # make request
    rsps = lambda_gateway(rqst, None)

    # display response
    if rsps.get("statusCode") == 200:
        content = json.loads(rsps["body"])
        print(json.dumps(content, indent=2))
    else:
        print(json.dumps(rsps, indent=2))