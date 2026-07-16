from provisioner import lambda_gateway, get_affiliation

#
# Test Active Affiliation
#
def test_active(username):
    data = get_affiliation(username)
    assert data["active"]

#
# Test Max TTL
#
def test_max_ttl(username):
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[affiliate]',
                        "sub": f"{username}",
                        "aud": '[test]',
                    }
                }
            }
        },
        "rawPath": "/info",
        "body": '{"cluster":"test","node_capacity":"1","ttl":"1000"}'
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Max Node
#
def test_max_node(username):
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[affiliate]',
                        "sub": f"{username}",
                        "aud": '[test]',
                    }
                }
            }
        },
        "rawPath": "/info",
        "body": '{"cluster":"test","node_capacity":"100","ttl":"15"}'
    }, None)
    assert rsps['statusCode'] == 403

#
# Test Invalid Cluster
#
def test_invalid_cluster(username):
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": '[affiliate]',
                        "sub": f"{username}",
                        "aud": '[test]',
                    }
                }
            }
        },
        "rawPath": "/info",
        "body": '{"cluster":"myowncluster","node_capacity":"1","ttl":"15"}'
    }, None)
    assert rsps['statusCode'] == 403
