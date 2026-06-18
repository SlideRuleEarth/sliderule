import json
from mcp import lambda_gateway

def construct_request(roles, path, method, parms, id):
    return {
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": f"[{' '.join(roles)}]"
                    }
                }
            }
        },
        "rawPath": json.dumps(path),
        "body": json.dumps({
            "method": method,
            "params": parms,
            "id": id
        })
    }
#
# Test Membership
#
def test_not_member():
    rsps = lambda_gateway(construct_request([], "/sliderule", "tools/call", {}, 1), None)
    error = json.loads(rsps["body"])['error']
    assert rsps['statusCode'] == 200, f"{rsps}"
    assert error['code'] == -32600, f"{error}"


#
# Test Path
#
def test_invalid_path():
    rsps = lambda_gateway(construct_request(["member"], "/does_not_exist", "tools/call", {}, 1), None)
    assert rsps['statusCode'] == 404, f"{rsps}"
