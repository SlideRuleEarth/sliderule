import sliderule
from datetime import datetime, timezone
from cryptography.hazmat.primitives.serialization import load_ssh_public_key
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat
import boto3
import json
import base64
import os


def verify_signature(username, domain, path, body, timestamp, signature_b64):
    try:

        # decode signature
        signature = base64.b64decode(signature_b64)

        # get public key
        sm = boto3.client('secretsmanager')
        secret_name = f"{domain}/pubkeys"
        secret_string = sm.get_secret_value(SecretId=secret_name)['SecretString']
        pubkey_cache = json.loads(secret_string)
        public_key = load_ssh_public_key(pubkey_cache[username].encode("utf-8"))

        # build canonical message
        full_path = f'{domain}/{path}'
        full_path_b64 = base64.urlsafe_b64encode(full_path.encode()).decode()
        body_b64 = base64.urlsafe_b64encode(body.encode()).decode()
        canonical_string = f"{full_path_b64}:{timestamp}:{body_b64}"
        message_bytes = canonical_string.encode("utf-8")

        # print values being used in verification
        public_key_bytes = public_key.public_bytes(Encoding.Raw, PublicFormat.Raw)
        print("SIGNATURE:", " ".join(f"{b:02X}" for b in signature))
        print("PUBLIC KEY:", " ".join(f"{b:02X}" for b in public_key_bytes))
        print(f"MESSAGE[{len(message_bytes)}]:", " ".join(f"{b:02X}" for b in message_bytes))
        print(f"CANONICAL STRING:", canonical_string)
        print(f"FULL PATH:", full_path)

        # verify message (raises exception if invalid)
        public_key.verify(signature, message_bytes)

        # check timestamp
        allowed_range_seconds = 60
        now = int(datetime.now(timezone.utc).timestamp())
        time_of_signature = int(timestamp)
        if (time_of_signature < (now - allowed_range_seconds)) or (time_of_signature > (now + allowed_range_seconds)):
            raise RuntimeError(f"Invalid time of signature: {time_of_signature}")

        # valid signature
        return True

    except Exception as e:

        # invalid signature
        print(f"Failed to verify request signature: {e}")
        return False

# Test Key Signing
#
def test_key_signing(domain, organization, username):
    session = sliderule.create_session(domain=domain, cluster=organization)
    session.domain = "slideruleearth.io"
    session.authenticate()
    session.domain = domain
    headers = {}
    api = "ace"
    path = f"source/{api}"
    body = 'return "Hello World"'
    session._Session__signrequest(headers, f"{domain}/{path}", body)
    rsps = session.source(api, body, sign=False, headers=headers)
    assert rsps == "Hello World".encode("ascii")
    assert verify_signature(username, domain, path, body, headers["x-sliderule-timestamp"], headers["x-sliderule-signature"])

