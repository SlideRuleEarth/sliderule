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
    "scope": 'sliderule:access'
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
        "scope":                    "sliderule:access",
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
def test_nominal(username):
    with open("/tmp/authenticator-pytest-username.txt", "w") as file:
        username = file.write(f"{username}")
    session = login()
    rqst = build_query_request('/auth/github/token', session)
    rsps = lambda_gateway(rqst, None)
    token_parms = json.loads(rsps['body'])
    print(token_parms)
    assert rsps['statusCode'] == 200, "make sure to run stub server"
    assert "access_token" in token_parms
    assert len(token_parms["access_token"]) > 0
    assert "token_type" in token_parms
    assert token_parms["token_type"] == 'Bearer'
    assert token_parms["scope"] == "sliderule:access"
    assert "affiliate" in token_parms["info"]["orgRoles"]