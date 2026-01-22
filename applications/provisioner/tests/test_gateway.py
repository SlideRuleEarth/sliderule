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
                        "org_roles": '["member"]',
                        "aud": '["gsfc"]'
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
                        "org_roles": '["member"]',
                        "aud": '["gsfc"]',
                        "max_nodes": "10"
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
                        "org_roles": '["member"]',
                        "aud": '["gsfc"]',
                        "max_nodes": "10",
                        "max_ttl": "600"
                    }
                }
            }
        },
        "rawPath": "/",
        "body": '{"cluster":"gsfc","node_capacity":"10","ttl":"700"}'
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Report
#
def test_invalid_report():
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '["member"]',
                        "aud": '["*"]',
                        "max_nodes": "10",
                        "max_ttl": "600"
                    }
                }
            }
        },
        "rawPath": "/report",
        "body": '{"cluster":"gsfc","node_capacity":"10","ttl":"600"}'
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
                        "org_roles": '["member", "owner"]',
                        "aud": '["*"]',
                        "max_nodes": "10",
                        "max_ttl": "600"
                    }
                }
            }
        },
        "rawPath": "/does_not_exist",
        "body": '{"cluster":"gsfc","node_capacity":"10","ttl":"600"}'
    }, None)
    assert rsps['statusCode'] == 404
