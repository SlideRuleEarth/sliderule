from github_oauth import lambda_gateway
from github_stub import *
from urllib.parse import urlparse, parse_qsl
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
    "scope": 'mcp:tools mcp:resources sliderule:access sliderule:admin'
})

register_rsps = json.loads(lambda_gateway(register_rqst, None)["body"])

code_verifier = generate_code_verifier()
code_challenge = generate_code_challenge(code_verifier)

state = "of-the-union"

def login():

    global register_rsps, redirect_uri, state, code_challenge

    login_rqst = build_query_request('/auth/github/login', {
        "response_type":            "code",
        "client_id":                register_rsps["client_id"],
        "redirect_uri":             redirect_uri,
        "state":                    state,
        "scope":                    "mcp:tools",
        "code_challenge":           code_challenge,
        "code_challenge_method":    'S256'
    })

    login_rsps = lambda_gateway(login_rqst, None)

    callback_rqst = build_query_request('/auth/github/callback', {
        "code": "super-secret-code",
        "state": dict(parse_qsl(urlparse(login_rsps['headers']['Location']).query))['state']
    })

    callback_rsps = lambda_gateway(callback_rqst, None)

    callback_parms = dict(parse_qsl(urlparse(callback_rsps['headers']['Location']).query))

    parms = {
        'grant_type': 'authorization_code',
        'code': callback_parms['code'],
        'redirect_uri': redirect_uri,
        'client_id': register_rsps["client_id"],
        'code_verifier': code_verifier,
    }

    return parms

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():
    session = login()
    rqst = build_query_request('/auth/github/token', session)
    rsps = lambda_gateway(rqst, None)
    token_parms = json.loads(rsps['body'])
    assert rsps['statusCode'] == 200, "make sure to run stub server"
    assert "access_token" in token_parms
    assert len(token_parms["access_token"]) > 0
    assert "token_type" in token_parms
    assert token_parms["token_type"] == 'Bearer'
    assert token_parms["scope"] == "mcp:tools"

#
# Test Back-To-Back Attempts
#
def test_back_to_back():
    session = login()
    rqst = build_query_request('/auth/github/token', session)
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 200
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Session
#
def test_invalid_session():
    session = login()
    rqst = build_query_request('/auth/github/token', session | {"client_id": "bogus"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Code
#
def test_invalid_code():
    session = login()
    rqst = build_query_request('/auth/github/token', session | {"code": "bogus"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Grant Type
#
def test_invalid_grant_type():
    session = login()
    rqst = build_query_request('/auth/github/token', session | {"grant_type": "general"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Redirect
#
def test_invalid_redirect():
    session = login()
    rqst = build_query_request('/auth/github/token', session | {"redirect_uri": "no-where-fast"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Verifier
#
def test_invalid_verifier():
    session = login()
    rqst = build_query_request('/auth/github/token', session | {"code_verifier": "just-my-own"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500
