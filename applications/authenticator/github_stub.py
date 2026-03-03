from fastapi import FastAPI, Request
import hashlib
import os
import base64
import json

# #######################
# Globals
# #######################

app = FastAPI()

# #######################
# Helper Functions
# #######################

def generate_code_verifier() -> str:
    """
    Generate a cryptographically random code_verifier.

    OAuth 2.1 / RFC 7636 requirements:
      - 43–128 characters
      - Characters from: A-Z a-z 0-9 - . _ ~  (unreserved URI chars)

    Using 32 random bytes → 43-char base64url string (no padding), which
    sits comfortably in the middle of the allowed range.
    """
    return base64.urlsafe_b64encode(os.urandom(32)).rstrip(b'=').decode()

def generate_code_challenge(code_verifier: str) -> str:
    """
    Derive the code_challenge from the verifier using S256.

    S256 is required by OAuth 2.1 (plain is deprecated).
    code_challenge = BASE64URL(SHA-256(ASCII(code_verifier)))
    """
    digest = hashlib.sha256(code_verifier.encode('ascii')).digest()
    return base64.urlsafe_b64encode(digest).rstrip(b'=').decode()

def build_query_request(path, query=None):
    return {
        "rawPath": path,
        "queryStringParameters": (query != None) and query or {}
    }

def build_body_request(path, body=None):
    return {
        "rawPath": path,
        "body": (body != None) and json.dumps(body) or ''
    }

def check_dictionary(superset, subset):
    status = True
    for x,y in subset.items():
        if superset[x].strip() != y.strip():
            print(f"Mismatch in {x}: {y} != {superset[x]}")
            status = False
            break
    return status

# #######################
# Test Endpoints
# #######################

@app.post("/test")
async def handle_test(request: Request):
    body = await request.json()
    return {
        "received": body
    }