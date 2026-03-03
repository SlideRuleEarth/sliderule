from github_oauth import lambda_gateway
from datetime import datetime
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
# Globals
# #######################

parms = {
    "redirect_uris": ["https://localhost/myapp"],
    "client_name": 'Unknown Client',
    "grant_types": ['authorization_code'],
    "response_types": ['code'],
    "auth_method": 'none',
    "challenge_method": 'S256',
    "scope": 'mcp:tools mcp:resources'
}

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():
    rqst = build_query_request('/auth/github/register', parms)
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 201
    assert check_dictionary(rsps['headers'], {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-store',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    })
    body = json.loads(rsps['body'])
    assert len(body["client_id"]) > 0
    assert int(body["client_id_issued_at"]) <= int(datetime.now().timestamp())
    assert body["client_name"] == "Unknown Client"
    assert len(body["redirect_uris"]) == 1
    assert body["redirect_uris"][0] == "https://localhost/myapp"
    assert len(body["grant_types"]) == 1
    assert body["grant_types"][0] == "authorization_code"
    assert len(body["response_types"]) == 1
    assert body["response_types"][0] == "code"
    assert body["token_endpoint_auth_method"] == "none"
    assert body["code_challenge_method"] == "S256"
    assert len(body["scope"]) == 2
    assert body["scope"][0] == "mcp:tools"
    assert body["scope"][1] == "mcp:resources"

#
# Test Invalid Redirect
#
def test_invalid_redirect():
    # invalid type
    rqst = build_query_request('/auth/github/register', parms | {"redirect_uris": {}})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
    # empty
    rqst = build_query_request('/auth/github/register', parms | {"redirect_uris": []})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
    # invalid entry - not https
    rqst = build_query_request('/auth/github/register', parms | {"redirect_uris": ["http://mydomain.com"]})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
    # invalid entry - type
    rqst = build_query_request('/auth/github/register', parms | {"redirect_uris": [1234]})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400

#
# Test Invalid Client Name
#
def test_invalid_client_name():
    # invalid type
    rqst = build_query_request('/auth/github/register', parms | {"client_name": {}})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
    # too long
    rqst = build_query_request('/auth/github/register', parms | {"client_name": "long"*100})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400

#
# Test Invalid Grant Types
#
def test_invalid_grant_types():
    # invalid type
    rqst = build_query_request('/auth/github/register', parms | {"grant_types": {}})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
    # not allowed
    rqst = build_query_request('/auth/github/register', parms | {"grant_types": "my grant"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400

#
# Test Invalid Response Types
#
def test_invalid_response_types():
    # invalid type
    rqst = build_query_request('/auth/github/register', parms | {"response_types": {}})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
    # not allowed
    rqst = build_query_request('/auth/github/register', parms | {"response_types": "my response"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400

#
# Test Invalid Auth Method
#
def test_invalid_auth_method():
    # not allowed
    rqst = build_query_request('/auth/github/register', parms | {"token_endpoint_auth_method": "my auth"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400

#
# Test Invalid Challenge Method
#
def test_invalid_challenge_method():
    # not allowed
    rqst = build_query_request('/auth/github/register', parms | {"code_challenge_method": "my challenge"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400

#
# Test Invalid Scope
#
def test_invalid_scope():
    # invalid type
    rqst = build_query_request('/auth/github/register', parms | {"scope": 5})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 400
