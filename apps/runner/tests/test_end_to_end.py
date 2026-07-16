from runner import lambda_gateway
import sliderule
import json

session = sliderule.create_session(domain="slideruleearth.io", cluster=None)
session.authenticate()

#
# Test Key Signing
#
def test_key_signing(username):
    body = json.dumps({"job_state": ["SUBMITTED"]})
    headers = {}
    session._Session__signrequest(headers, "slideruleearth.io/report/queue", body)
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": "[member]",
                        "sub": username
                    }
                }
            }
        },
        "headers": {
            "host": "slideruleearth.io",
            "x-sliderule-timestamp": headers["x-sliderule-timestamp"],
            "x-sliderule-signature": headers["x-sliderule-signature"]
        },
        "rawPath": "/report/queue",
        "body": body
    }, None)
    assert rsps['statusCode'] == 200

#
# Test Membership
#
def test_not_member(username):
    body = json.dumps({"job_state": ["SUBMITTED"]})
    headers = {}
    session._Session__signrequest(headers, "slideruleearth.io/report/queue", body)
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": "[]",
                        "sub": username
                    }
                }
            }
        },
        "headers": {
            "host": "slideruleearth.io",
            "x-sliderule-timestamp": headers["x-sliderule-timestamp"],
            "x-sliderule-signature": headers["x-sliderule-signature"]
        },
        "rawPath": "/report/queue",
        "body": body
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Path
#
def test_invalid_path(username):
    headers = {}
    session._Session__signrequest(headers, "slideruleearth.io/does-not-exist", "")
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": "[member]",
                        "sub": username
                    }
                }
            }
        },
        "headers": {
            "host": "slideruleearth.io",
            "x-sliderule-timestamp": headers["x-sliderule-timestamp"],
            "x-sliderule-signature": headers["x-sliderule-signature"]
        },
        "rawPath": "/does-not-exist",
        "body": ""
    }, None)
    assert rsps['statusCode'] == 404
