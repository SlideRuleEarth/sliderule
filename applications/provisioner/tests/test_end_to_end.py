from provisioner import lambda_gateway
from pathlib import Path
from datetime import datetime, timezone
from cryptography.hazmat.primitives.serialization import load_ssh_private_key
import base64
import os

#
# Test Key Signing
#
def test_key_signin(username):
    with open(os.path.join(Path.home(), ".sliderule_key"), "rb") as file:
        private_key = load_ssh_private_key(file.read(), password=None)
    path = f'slideruleearth.io/info'
    path_b64 = base64.urlsafe_b64encode(path.encode()).decode()
    timestamp = str(int(datetime.now(timezone.utc).timestamp()))
    body_b64 = base64.urlsafe_b64encode(''.encode()).decode()
    canonical_string = f"{path_b64}:{timestamp}:{body_b64}"
    message_bytes = canonical_string.encode("utf-8")
    signature = private_key.sign(message_bytes)
    signature_b64 = base64.b64encode(signature).decode("ascii")
    rsps = lambda_gateway({
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": "[member owner]",
                        "aud": "[*]",
                        "sub": username
                    }
                }
            }
        },
        "headers": {
            "host": "slideruleearth.io",
            "X-SlideRule-Timestamp": str(timestamp),
            "X-SlideRule-Signature": signature_b64
        },
        "rawPath": "/info",
        "body": ""
    }, None)
    assert rsps['statusCode'] == 200
