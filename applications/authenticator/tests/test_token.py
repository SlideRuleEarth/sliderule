#
# In order to run the tests in this file, some additional external steps need to be taken:
#   1. The following "stub" server must be run: `uvicorn github_stub:app --port 9083 --reload`
#   2. The JWT_SIGNING_KEY_ARN needs to be retrieved: `make test-authenticator-env`
#   3. Run the pytest with the jwt key arn passed in: `JWT_SIGNING_KEY_ARN=<arn> pytest`
#

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

# #######################
# Test Functions
# #######################

#
# Test Nominal
#
def test_nominal():
    rqst = build_query_request('/auth/github/token', parms)
    rsps = lambda_gateway(rqst, None)
    assert rsps['statusCode'] == 200, "make sure to run stub server"
