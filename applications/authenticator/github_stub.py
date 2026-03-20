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
      - 43-128 characters
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

@app.post("/login/oauth/access_token")
async def handle_access_token(request: Request):
    form = await request.form()
    data = dict(form)
    return {
        "received": data,
        "access_token": "stubbed-token"
    }

@app.get("/api/user")
async def handle_api_user(request: Request):
    form = await request.form()
    data = dict(form)
    try:
        tmp_username_file = "/tmp/authenticator-pytest-username.txt"
        with open(tmp_username_file, "r") as file:
            username = file.read()
        if os.path.exists(tmp_username_file):
            os.remove(tmp_username_file)
    except:
        username = "my-user-name"
    return {
        "received": data,
        "login": username
    }

@app.get("/api/orgs/myorg/memberships/{username}")
async def handle_api_memberships(username: str, request: Request):
    form = await request.form()
    data = dict(form)
    return {
        "received": data,
        "state": "active",
        "role": "admin"
    }

@app.get("/api/user/teams")
async def handle_api_teams(page: int = 1, per_page: int = 30):
    print("TEAMS", page, per_page)
    if page <= 1:
        return [{
            "page": page,
            "per_page": per_page,
            "organization": {
                'login': 'myorg',
                'slug': 'myteam'
            }
        }]
    else:
        return {}
