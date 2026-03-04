from github_oauth import lambda_gateway
from github_stub import *
from datetime import datetime
import json

# #######################
# Globals
# #######################

redirect_uri = "https://localhost/myapp"

register_rqst = build_query_request('/auth/github/register', {
    "redirect_uris": [redirect_uri],
    "client_name": 'Unknown Client',
    "grant_types": ['authorization_code'],
    "response_types": ['code'],
    "auth_method": 'none',
    "challenge_method": 'S256',
    "scope": 'mcp:tools mcp:resources'
})
register_rsps = json.loads(lambda_gateway(register_rqst, None)["body"])

code_verifier = generate_code_verifier()
code_challenge = generate_code_challenge(code_verifier)

parms = {
    "response_type":            "code",
    "client_id":                register_rsps["client_id"],
    "redirect_uri":             redirect_uri,
    "client_state":             "mystate",
    "scope":                    "mcp:tools",
    "code_challenge":           code_challenge,
    "code_challenge_method":    'S256'
}

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():
    rqst = build_query_request('/auth/github/login', parms)
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 302
    assert check_dictionary(rsps['headers'], {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    })
    assert 'https://localhost:9083/login/oauth/authorize?client_id=myid' in rsps['headers']['Location']

#
# Test Invalid Session
#
def test_invalid_session():
    rqst = build_query_request('/auth/github/login', parms | {"client_id": "my fake id"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Redirect
#
def test_invalid_redirect():
    rqst = build_query_request('/auth/github/login', parms | {"redirect_uri": "goes nowhere"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Response Type
#
def test_invalid_response_type():
    rqst = build_query_request('/auth/github/login', parms | {"response_type": "dont talk back"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Challenge Method
#
def test_invalid_challenge_method():
    rqst = build_query_request('/auth/github/login', parms | {"code_challenge_method": "disrespect"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Scope
#
def test_invalid_challenge_method():
    rqst = build_query_request('/auth/github/login', parms | {"scope": "doctors"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500
