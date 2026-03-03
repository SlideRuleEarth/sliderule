from github_oauth import lambda_gateway
import json

# #######################
# Helper Functions
# #######################

def build_query_request(path, query=None):
    return {
        "rawPath": path,
        "queryStringParameters": (query != None) and query or {}
    }

def build_body_request(path, body=None):
    return {
        "rawPath": path,
        "body": (body != None) and json.dumps(body) or ''
    }

def check_dictionary(superset, subset):
    status = True
    for x,y in subset.items():
        if superset[x].strip() != y.strip():
            print(f"Mismatch in {x}: {y} != {superset[x]}")
            status = False
            break
    return status

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():
    rqst = build_query_request('/auth/github/register', {
        "redirect_uris": ["https://localhost/myapp"],
        "client_name": 'Unknown Client',
        "grant_types": ['authorization_code'],
        "response_types": ['code'],
        "auth_method": 'none',
        "challenge_method": 'S256',
        "scope": 'mcp:tools mcp:resources'
    })
    print(rqst)
    rsps = lambda_gateway(rqst, None)
    print(rsps)
    assert rsps['statusCode'] == 201
    assert check_dictionary(rsps['headers'], {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-store',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    })

