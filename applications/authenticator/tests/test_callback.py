from github_oauth import lambda_gateway, create_signed_state
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
    "scope": 'mcp:tools mcp:resources sliderule:access sliderule:trusted'
})

register_rsps = json.loads(lambda_gateway(register_rqst, None)["body"])

code_verifier = generate_code_verifier()
code_challenge = generate_code_challenge(code_verifier)

state = "of-the-union"

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

callback_parms = dict(parse_qsl(urlparse(login_rsps['headers']['Location']).query))

parms = {
    "code": "super-secret-code",
    "state": callback_parms['state']
}

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():
    rqst = build_query_request('/auth/github/callback', parms)
    rsps = lambda_gateway(rqst, None)
    redirect_parms = dict(parse_qsl(urlparse(rsps['headers']['Location']).query))
    assert rsps['statusCode'] == 302
    assert check_dictionary(rsps['headers'], {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    })
    assert redirect_uri in rsps['headers']['Location']
    assert 'code' in redirect_parms
    assert len(redirect_parms['code']) > 0
    assert redirect_parms['state'] == state

#
# Test Invalid State
#
def test_invalid_state():
    rqst = build_query_request('/auth/github/callback', parms | {"state": "bogus"})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Session
#
def test_invalid_session():
    client_id = "bogus"
    redirect_uri_b64 = base64.urlsafe_b64encode(redirect_uri.encode()).decode()
    github_state = create_signed_state(f"{client_id}:{redirect_uri_b64}:{state}")
    rqst = build_query_request('/auth/github/callback', parms | {"state": github_state})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test Invalid Redirect
#
def test_invalid_redirect():
    bad_redirect = "bogus"
    redirect_uri_b64 = base64.urlsafe_b64encode(bad_redirect.encode()).decode()
    github_state = create_signed_state(f"{register_rsps["client_id"]}:{redirect_uri_b64}:{state}")
    rqst = build_query_request('/auth/github/callback', parms | {"state": github_state})
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 500

#
# Test GitHub Error
#
def test_github_error():
    rqst = build_query_request('/auth/github/callback', parms | {"error": "it happens sometimes"})
    rsps = lambda_gateway(rqst, None)
    redirect_parms = dict(parse_qsl(urlparse(rsps['headers']['Location']).query))
    assert rsps['statusCode'] == 302
    assert 'error' in redirect_parms
    assert 'code' not in redirect_parms
    assert 'state' not in redirect_parms

#
# Test Missing Code
#
def test_missing_code():
    rqst = build_query_request('/auth/github/callback', {"state": callback_parms['state']})
    rsps = lambda_gateway(rqst, None)
    redirect_parms = dict(parse_qsl(urlparse(rsps['headers']['Location']).query))
    assert rsps['statusCode'] == 302
    assert 'error' in redirect_parms
    assert 'code' not in redirect_parms
    assert 'state' not in redirect_parms
