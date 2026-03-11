from github_oauth import lambda_gateway
from github_stub import *
from urllib.parse import urlparse, parse_qsl
import json

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():

    redirect_uri = "https://companyx.com/myapp"

    register_rqst = build_query_request('/auth/github/register', {
        "redirect_uris": [redirect_uri],
        "client_name": 'Unknown Client',
        "grant_types": ['authorization_code'],
        "response_types": ['code'],
        "auth_method": 'none',
        "challenge_method": 'S256',
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
        "code_challenge":           code_challenge,
        "code_challenge_method":    'S256',
        "resource":                 f"https://mcp.localhost/mcp"
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

    rqst = build_query_request('/auth/github/token', parms)
    rsps = lambda_gateway(rqst, None)
    token_parms = json.loads(rsps['body'])
    token = token_parms["access_token"]

    payload = token.split(".")[1]
    payload += "=" * (4 - len(payload) % 4)
    decoded = base64.urlsafe_b64decode(payload)
    claims = json.loads(decoded)

    assert rsps['statusCode'] == 200, "make sure to run stub server"
    assert token_parms["scope"] == "mcp:resources"
    assert len(claims["aud"]) == 1
    assert claims["aud"][0] == f"https://mcp.{os.environ.get("DOMAIN")}/mcp"

#
# Test Invalid Scope
#
def test_invalid_scope():

    redirect_uri = "https://companyx.com/myapp"

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
        "code_challenge":           code_challenge,
        "code_challenge_method":    'S256',
        "scope":                    "sliderule:access",
        "resource":                 f"https://mcp.localhost/mcp"
    })

    login_rsps = lambda_gateway(login_rqst, None)

    callback_rqst = build_query_request('/auth/github/callback', {
        "code": "super-secret-code",
        "state": dict(parse_qsl(urlparse(login_rsps['headers']['Location']).query))['state']
    })

    callback_rsps = lambda_gateway(callback_rqst, None)

    assert callback_rsps['statusCode'] == 500

