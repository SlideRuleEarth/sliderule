import json

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
        "rawPath": path,
        "body": json.dumps({
            "method": method,
            "params": parms,
            "id": id
        })
    }