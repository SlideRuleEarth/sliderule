from runner import lambda_gateway

#
# Test Membership
#
def test_not_member():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": "[]"
                    }
                }
            }
        },
        "rawPath": "/",
        "body": ""
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Path
#
def test_invalid_path():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member owner]',
                        "aud": '[*]'
                    }
                }
            }
        },
        "rawPath": "/does_not_exist",
        "body": '{}'
    }, None)
    assert rsps['statusCode'] == 404
