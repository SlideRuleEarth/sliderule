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
    assert rsps['statusCode'] == 404

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
# Test Spot Capacity
#
def test_invalid_spot_capacity():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]',
                        "aud": '[gsfc other]',
                        "max_nodes": "20"
                    }
                }
            }
        },
        "rawPath": "/",
        "body": '{"cluster":"gsfc","node_capacity":"10","spot_capacity":"11"}'
    }, None)
    assert rsps['statusCode'] == 403 # node + spot capacity exceeds max nodes

def test_request_without_capacity():
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
        "rawPath": "/info",
        "body": '{"cluster":"gsfc"}'
    }, None)
    assert rsps['statusCode'] == 200 # non-deploy requests don't carry a node capacity

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
        "rawPath": "/report/clusters",
        "body": '{"cluster":"gsfc","node_capacity":"10","ttl":"600"}'
    }, None)
    assert rsps['statusCode'] == 403 # ["*"] requires signed request

#
# Test Path
#
def test_invalid_path():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[member]',
                        "max_nodes": 10,
                        "max_ttl": 600
                    }
                }
            }
        },
        "rawPath": "/does_not_exist",
        "body": '{"node_capacity":"10","ttl":"600"}'
    }, None)
    assert rsps['statusCode'] == 404, f"{rsps}"
