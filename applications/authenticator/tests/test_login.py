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
    print(rsps)
    assert rsps['statusCode'] == 302
    assert check_dictionary(rsps['headers'], {
        'Content-Type': 'application/json',
        'Cache-Control': 'no-cache, no-store, must-revalidate',
        'Access-Control-Allow-Origin': '*',
        'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
        'Access-Control-Allow-Headers': 'Content-Type'
    })
