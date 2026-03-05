from provisioner import lambda_gateway

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
# Test Cluster
#
def test_invalid_cluster():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]',
                        "aud": '[gsfc]'
                    }
                }
            }
        },
        "rawPath": "/",
        "body": '{"cluster":"mycluster"}'
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Node Capacity
#
def test_invalid_node_capacity():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]',
                        "aud": '[gsfc other]',
                    }
                }
            }
        },
        "rawPath": "/",
        "body": '{"cluster":"gsfc","node_capacity":"60"}'
    }, None)
    assert rsps['statusCode'] == 403

#
# Test TTL
#
def test_invalid_ttl():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]',
                        "aud": '[gsfc other]',
                    }
                }
            }
        },
        "rawPath": "/",
        "body": '{"cluster":"gsfc","node_capacity":"10","ttl":"800"}'
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Unpermitted Admin Request
#
def test_admin_request():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]',
                        "aud": '[*]'
                    }
                }
            }
        },
        "rawPath": "/report",
        "body": '{"cluster":"gsfc","node_capacity":"10","ttl":"600"}'
    }, None)
    assert rsps['statusCode'] == 403 # ["*"] requires signed request

#
# Test Report
#
def test_invalid_report():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]'
                    }
                }
            }
        },
        "rawPath": "/report",
        "body": '{"node_capacity":"10","ttl":"600"}'
    }, None)
    assert rsps['statusCode'] == 404 # api not exposed

#
# Test Path
#
def test_invalid_path():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]'
                    }
                }
            }
        },
        "rawPath": "/does_not_exist",
        "body": '{"node_capacity":"10","ttl":"600"}'
    }, None)
    assert rsps['statusCode'] == 404, f"{rsps}"
