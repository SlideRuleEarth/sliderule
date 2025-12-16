"""
GitHub OAuth Lambda Handler for SlideRule

Supports two flows:
1. Authorization Code Flow (for web clients)
2. Device Flow (for CLI/Python clients)
"""

import base64
import hashlib
import hmac
import json
import os
import secrets
import urllib.parse
from datetime import datetime, timedelta, timezone
import boto3
import requests

############################
# GLOBALS
############################

# Configuration from environment variables
GITHUB_ORG = os.environ.get('GITHUB_ORG')
GITHUB_CLIENT_ID = os.environ.get('GITHUB_CLIENT_ID')
CLIENT_SECRET_NAME = os.environ.get('CLIENT_SECRET_NAME')
JWT_SIGNING_KEY_ARN = os.environ.get('JWT_SIGNING_KEY_ARN') # KMS key ARN for JWT signing (RS256 asymmetric)
HMAC_SIGNING_KEY_ARN = os.environ.get('HMAC_SIGNING_KEY_ARN') # Secrets Manager ARN for HMAC key (OAuth state signing)
ALLOWED_REDIRECT_HOSTS = os.environ.get('ALLOWED_REDIRECT_HOSTS', '').split(' ') # Validated against the redirect_uri to prevent attackers from redirecting tokens to malicious sites
JWT_EXPIRATION_HOURS = int(os.environ.get('JWT_EXPIRATION_HOURS', '12'))
JWT_AUDIENCE = os.environ.get('JWT_AUDIENCE') # JWT audience claim - services validating the token should check this

# JWT configuration
JWT_ALGORITHM = 'RS256'

# KMS client for JWT signing (initialized lazily)
_kms_client = None

# HTTP request timeout (seconds) - prevents Lambda from hanging on stalled connections
HTTP_TIMEOUT_SECONDS = 15

# OAuth state expiration (seconds) - state tokens older than this are rejected
STATE_EXPIRATION_SECONDS = 600  # 10 minutes

# GitHub OAuth endpoints
GITHUB_AUTHORIZE_URL = 'https://github.com/login/oauth/authorize'
GITHUB_TOKEN_URL = 'https://github.com/login/oauth/access_token'
GITHUB_DEVICE_CODE_URL = 'https://github.com/login/device/code'
GITHUB_API_URL = 'https://api.github.com'

# Cache for secrets (Lambda container reuse)
_secrets_cache = {}

# =============================================================================
# AWS Helper Functions
# =============================================================================

def _get_secret(secret_arn):
    """Retrieve a secret from AWS Secrets Manager."""
    client = boto3.client('secretsmanager')
    response = client.get_secret_value(SecretId=secret_arn)
    return response['SecretString']


def get_github_client_secret():
    """Retrieve GitHub client secret from AWS Secrets Manager."""
    if 'client_secret' not in _secrets_cache:
        if not CLIENT_SECRET_NAME:
            raise ValueError("CLIENT_SECRET_NAME environment variable not set")
        _secrets_cache['client_secret'] = _get_secret(CLIENT_SECRET_NAME)
    return _secrets_cache['client_secret']


def get_hmac_signing_key():
    """
    Retrieve HMAC signing key from AWS Secrets Manager.
    Used for HMAC operations (OAuth state signing for CSRF protection).
    JWT signing uses KMS directly via sign_with_kms().
    """
    if 'hmac_signing_key' not in _secrets_cache:
        if not HMAC_SIGNING_KEY_ARN:
            raise ValueError("HMAC_SIGNING_KEY_ARN environment variable not set")
        _secrets_cache['hmac_signing_key'] = _get_secret(HMAC_SIGNING_KEY_ARN)
    return _secrets_cache['hmac_signing_key']


def get_kms_client():
    """Get or create KMS client (cached for Lambda container reuse)."""
    global _kms_client
    if _kms_client is None:
        _kms_client = boto3.client('kms')
    return _kms_client


def sign_with_kms(message_bytes):
    """
    Sign a message using AWS KMS asymmetric key.

    Uses RSASSA_PKCS1_V1_5_SHA_256 algorithm which corresponds to RS256 in JWT.
    The private key never leaves KMS - signing happens server-side.

    Args:
        message_bytes: The message to sign (bytes)

    Returns:
        The signature as bytes
    """
    if not JWT_SIGNING_KEY_ARN:
        raise ValueError("JWT_SIGNING_KEY_ARN environment variable not set")

    kms = get_kms_client()
    response = kms.sign(
        KeyId=JWT_SIGNING_KEY_ARN,
        Message=message_bytes,
        MessageType='RAW',
        SigningAlgorithm='RSASSA_PKCS1_V1_5_SHA_256'
    )
    return response['Signature']


# =============================================================================
# HMAC State Helper Functions
# =============================================================================

def create_signed_state(redirect_uri=None):
    """
    Create an HMAC-signed OAuth state parameter for CSRF protection.

    The state includes:
    - A random nonce for uniqueness
    - A timestamp for expiration checking
    - Optional redirect URI
    - HMAC signature to prevent tampering

    Format: {nonce}:{timestamp}:{redirect_uri_b64}:{signature}

    Args:
        redirect_uri: Optional frontend redirect URI to include in state

    Returns:
        A signed state string
    """
    signing_key = get_hmac_signing_key()
    nonce = secrets.token_urlsafe(16)
    timestamp = str(int(datetime.now(timezone.utc).timestamp()))

    # Base64 encode redirect_uri to avoid delimiter issues
    redirect_uri_b64 = ''
    if redirect_uri:
        redirect_uri_b64 = base64.urlsafe_b64encode(redirect_uri.encode()).decode()

    # Create message to sign (nonce:timestamp:redirect_uri_b64)
    message = f"{nonce}:{timestamp}:{redirect_uri_b64}"

    # Create HMAC signature
    signature = hmac.new(
        signing_key.encode(),
        message.encode(),
        hashlib.sha256
    ).hexdigest()

    return f"{message}:{signature}"


def verify_signed_state(state):
    """
    Verify an HMAC-signed OAuth state parameter.

    Checks:
    - HMAC signature is valid (prevents tampering)
    - Timestamp is within STATE_EXPIRATION_SECONDS (prevents replay)

    Args:
        state: The state string from the OAuth callback

    Returns:
        Tuple of (is_valid: bool, redirect_uri: str | None, error: str | None)
    """
    if not state:
        return False, None, "Missing state parameter"

    try:
        parts = state.split(':')
        if len(parts) != 4:
            return False, None, "Invalid state format"

        nonce, timestamp_str, redirect_uri_b64, provided_signature = parts

        # Verify HMAC signature
        signing_key = get_hmac_signing_key()
        message = f"{nonce}:{timestamp_str}:{redirect_uri_b64}"
        expected_signature = hmac.new(
            signing_key.encode(),
            message.encode(),
            hashlib.sha256
        ).hexdigest()

        if not hmac.compare_digest(provided_signature, expected_signature):
            return False, None, "Invalid state signature"

        # Check timestamp expiration
        timestamp = int(timestamp_str)
        now = int(datetime.now(timezone.utc).timestamp())
        if now - timestamp > STATE_EXPIRATION_SECONDS:
            return False, None, "State has expired"

        # Decode redirect URI if present
        redirect_uri = None
        if redirect_uri_b64:
            redirect_uri = base64.urlsafe_b64decode(redirect_uri_b64.encode()).decode()

        return True, redirect_uri, None

    except (ValueError, TypeError, base64.binascii.Error) as e:
        print(f"Error verifying state: {e}")
        return False, None, "Invalid state format"


# =============================================================================
# GitHub Helper Functions
# =============================================================================

def get_github_user(access_token):
    """Get authenticated user's GitHub profile."""
    response = requests.get(
        f"{GITHUB_API_URL}/user",
        headers={
            'Authorization': f'Bearer {access_token}',
            'Accept': 'application/vnd.github.v3+json'
        },
        timeout=HTTP_TIMEOUT_SECONDS
    )

    if response.status_code != 200:
        raise Exception(f"Failed to get user info: {response.text}")

    return response.json()


def get_organization_roles(access_token, username):
    """
    Builds a list of roles for the user
    Raises:
        Exception: If GitHub API returns an unexpected error (5xx, 429, etc.)
                   This prevents silently degrading users to non-member status
                   during GitHub outages.
    """
    # Initialize return values
    is_org_owner = False
    is_org_member = False

    # Try to get the user's membership in the org
    response = requests.get(
        f"{GITHUB_API_URL}/orgs/{GITHUB_ORG}/memberships/{username}",
        headers={
            'Authorization': f'Bearer {access_token}',
            'Accept': 'application/vnd.github.v3+json'
        },
        timeout=HTTP_TIMEOUT_SECONDS
    )

    if response.status_code == 200:
        data = response.json()
        state = data.get('state', '')
        role = data.get('role', '')
        # User must have 'active' state to be considered a member
        is_org_member = state == 'active'
        is_org_owner = is_org_member and role == 'admin'
    elif response.status_code == 404:
        # User is not a member of the organization - this is expected for non-members
        pass
    elif response.status_code == 429:
        # Rate limit exceeded - don't silently degrade, surface the error
        raise Exception("GitHub API rate limit exceeded. Please try again later.")
    elif response.status_code >= 500:
        # GitHub API error - don't silently degrade, surface the error
        raise Exception(f"GitHub API is unavailable (status {response.status_code}). Please try again later.")
    else:
        # Other unexpected errors - fail explicitly rather than silently degrading
        print(f"Unexpected response checking org membership: {response.status_code} {response.text}")
        raise Exception(f"Failed to verify organization membership: GitHub returned status {response.status_code}")

    # Build organization roles
    roles = []
    if is_org_owner:
        roles = ['owner', 'member']
    elif is_org_member:
        roles = ['member']

    # Return organization roles
    return roles


def get_user_teams(access_token, username, org_roles):
    """
    Get all teams the user belongs to in the SlideRuleEarth organization.

    Uses the /user/teams endpoint which works for all authenticated users,
    unlike /orgs/{org}/teams which requires admin permissions.

    Returns a list of team slugs
    """
    # Initialize teams
    teams = []

    # Early check if not a member
    if 'member' not in org_roles:
        return teams

    # Use /user/teams which lists teams for the authenticated user
    # This works for all users, unlike /orgs/{org}/teams which requires admin
    url = f"{GITHUB_API_URL}/user/teams"
    page = 1
    per_page = 100

    while True:
        # Make request to GitHub
        response = requests.get(
            url,
            headers={
                'Authorization': f'Bearer {access_token}',
                'Accept': 'application/vnd.github.v3+json'
            },
            params={
                'page': page,
                'per_page': per_page
            },
            timeout=HTTP_TIMEOUT_SECONDS
        )

        # Check valid response
        if response.status_code != 200:
            print(f"Failed to get user teams: {response.status_code} {response.text}")
            break

        # Pull out teams
        user_teams = response.json()
        for team in user_teams:
            org = team.get('organization', {})
            if org.get('login') == GITHUB_ORG:
                team_slug = team.get('slug')
                if team_slug:
                    teams.append(team_slug)

        # Check if there are more pages
        if not user_teams:
            break
        if len(user_teams) < per_page:
            break
        page += 1

    # Return teams
    return teams


# =============================================================================
# Business Logic for Generating Tokens and Metadata
# =============================================================================

def get_allowed_clusters(username, teams, org_roles):
    """
    Returns a list of cluster names the user can deploy.
    """
    # Initialize deployable clusters
    deployable = []

    # Early check if member
    if 'member' not in org_roles:
        return deployable

    # All members can deploy to their personal cluster
    if username:
        deployable.append(f"{username}-cluster")

    # All members can deploy to their team clusters
    if teams:
        deployable.extend(teams)

    # Owners can deploy anything
    if 'owner' in org_roles:
        deployable.extend(['*'])

    # Return deployable clusters
    return deployable


def get_max_nodes(org_roles):
    """
    Returns the maximum number of nodes a user can deploy
    """
    max_nodes = 0
    if 'owner' in org_roles:
        max_nodes = 500
    elif 'member' in org_roles:
        max_nodes = 10
    return max_nodes


def get_max_ttl(org_roles):
    """
    Returns the maximum time to live a user can deploy
    """
    max_ttl = 0
    if 'owner' in org_roles:
        max_ttl = 525600 # 1 year
    elif 'member' in org_roles:
        max_ttl = 720 # 12 hours
    return max_ttl


def create_auth_token(metadata):
    """
    Create a minimal signed JWT containing only server-essential fields.
    This token is validated server-side only; clients should treat it as opaque.
    """
    # Build JWT header
    header = {
        'alg': JWT_ALGORITHM,
        'typ': 'JWT'
    }

    # Helper Function
    def base64url_encode(data):
        return base64.urlsafe_b64encode(data).rstrip(b'=').decode('ascii')

    # Encode header and payload (base64url without padding)
    header_b64 = base64url_encode(json.dumps(header, separators=(',', ':')).encode('utf-8'))
    payload_b64 = base64url_encode(json.dumps(metadata, separators=(',', ':')).encode('utf-8'))

    # Create signing input
    signing_input = f"{header_b64}.{payload_b64}"

    # Sign with KMS (RS256 = RSASSA-PKCS1-v1_5 using SHA-256)
    signature = sign_with_kms(signing_input.encode('utf-8'))
    signature_b64 = base64url_encode(signature)

    # Assemble final JWT
    token = f"{signing_input}.{signature_b64}"
    return token


def authenticate_user(access_token, event):
    """
    Build the authentication token and metadata for the user
    """
    # Get user info
    user_info = get_github_user(access_token)

    # Get username
    username = user_info.get('login')
    if not username:
        raise RuntimeError('Could not get GitHub username')

    # Get user metadata
    org_roles = get_organization_roles(access_token, username)
    teams = get_user_teams(access_token, username, org_roles)
    allowed_clusters = get_allowed_clusters(username, teams, org_roles)
    max_nodes = get_max_nodes(org_roles)
    max_ttl = get_max_ttl(org_roles)

    # Token expiration based on JWT_EXPIRATION_HOURS config
    now = datetime.now(timezone.utc)
    expiration = now + timedelta(hours=JWT_EXPIRATION_HOURS)

    # Build issuer URL from API host (for JWKS discovery at {iss}/.well-known/jwks.json)
    headers = event.get('headers', {})
    api_host = headers.get('host', '')
    issuer = f"https://{api_host}"

    # Build metadata dictionary
    metadata = {
        'username': username,
        'org_roles': org_roles,
        'teams': teams,
        'allowed_clusters': allowed_clusters,
        'max_nodes': max_nodes,
        'max_ttl': max_ttl,
        'sub': username,
        'aud': JWT_AUDIENCE,
        'org': GITHUB_ORG,
        'iat': int(now.timestamp()),
        'exp': int(expiration.timestamp()),
        'iss': issuer
    }

    # Create minimal signed JWT token (only server-essential fields)
    auth_token = create_auth_token(metadata=metadata)

    # Return artifacts
    return auth_token, metadata


# =============================================================================
# GitHub Oauth Web Login Flow
# =============================================================================

def handle_login(event):
    """
    Initiate GitHub OAuth flow.
    Redirects user to GitHub authorization page.

    Creates an HMAC-signed state parameter that includes:
    - Random nonce for uniqueness
    - Timestamp for expiration
    - Optional redirect URI
    - Cryptographic signature for CSRF protection
    """
    # Get query parameters
    query_params = event.get('queryStringParameters') or {}
    redirect_uri = query_params.get('redirect_uri')

    # Build the callback URL (this Lambda's callback endpoint)
    headers = event.get('headers', {})
    host = headers.get('host', '')
    protocol = 'https'
    callback_url = f"{protocol}://{host}/auth/github/callback"

    # Create HMAC-signed state for CSRF protection
    # The state includes the redirect_uri securely encoded
    state = create_signed_state(redirect_uri)

    # Build GitHub authorization URL
    params = {
        'client_id': GITHUB_CLIENT_ID,
        'redirect_uri': callback_url,
        'scope': 'read:org',
        'state': state
    }
    auth_url = f"{GITHUB_AUTHORIZE_URL}?{urllib.parse.urlencode(params)}"

    # Return response
    return {
        'statusCode': 302,
        'headers': {
            'Location': auth_url,
            'Cache-Control': 'no-cache, no-store, must-revalidate'
        },
        'body': ''
    }


def handle_callback(event):
    """
    Handle GitHub OAuth callback.
    Exchange code for token, build metadata, redirect to frontend.
    Security: Validates HMAC-signed state parameter to prevent CSRF attacks.
    """
    query_params = event.get('queryStringParameters') or {}
    code = query_params.get('code')
    state = query_params.get('state', '')
    error = query_params.get('error')
    error_description = query_params.get('error_description')

    # Verify state parameter FIRST (CSRF protection)
    is_valid, redirect_uri, state_error = verify_signed_state(state)
    if not is_valid:
        raise RuntimeError(f"Security: Invalid OAuth state - {state_error}")

    # Handle OAuth errors from GitHub
    if error:
        return redirect_to_frontend(redirect_uri, {'error': error_description or error})
    if not code:
        return redirect_to_frontend(redirect_uri, {'error': 'No authorization code received'})

    try:
        # Authenticate user (gets token and metadata)
        access_token = exchange_code_for_token(code, event)
        token, metadata = authenticate_user(access_token, event)

        # Build known clusters (what the user can possibly connect to)
        known_clusters = ['sliderule'] + [cluster for cluster in metadata['allowed_clusters'] if cluster != "*"]

        # Build parameters for callback
        parms = {
            'username': metadata['username'],
            'isOrgMember': 'true' if ('member' in metadata["org_roles"]) else 'false',
            'isOrgOwner': 'true' if ('owner' in metadata["org_roles"]) else 'false',
            'org': metadata['org'],
            'teams': ','.join(metadata['teams']),
            'orgRoles': ','.join(metadata['org_roles']),
            'knownClusters': ','.join(known_clusters),
            'deployableClusters': ','.join(metadata['allowed_clusters']),
            'maxNodes': str(metadata['max_nodes']),
            'maxTTL': str(metadata['max_ttl']),
            'tokenIssuedAt': str(metadata['iat']),
            'tokenExpiresAt': str(metadata['exp']),
            'tokenIssuer': metadata['iss'],
            'token': token
        }

        # Redirect to frontend with results
        return redirect_to_frontend(redirect_uri, parms)

    except Exception as e:
        print('Exception in OAuth callback: {e}')
        return redirect_to_frontend(redirect_uri, {'error': f"Error during OAuth callback"})


def exchange_code_for_token(code, event):
    """Exchange authorization code for access token."""
    headers = event.get('headers', {})
    host = headers.get('host', '')

    if not is_allowed_host(host):
        raise RuntimeError(F"Invalid host: {host}")

    callback_url = f"https://{host}/auth/github/callback"
    client_secret = get_github_client_secret()

    response = requests.post(
        GITHUB_TOKEN_URL,
        headers={
            'Accept': 'application/json',
            'Content-Type': 'application/x-www-form-urlencoded'
        },
        data={
            'client_id': GITHUB_CLIENT_ID,
            'client_secret': client_secret,
            'code': code,
            'redirect_uri': callback_url
        },
        timeout=HTTP_TIMEOUT_SECONDS
    )

    if response.status_code != 200:
        raise Exception(f"Failed to exchange code for token: {response.text}")

    data = response.json()

    if 'error' in data:
        raise Exception(f"GitHub OAuth error: {data.get('error_description', data['error'])}")

    access_token = data.get('access_token')
    if not access_token:
        raise Exception("No access token in response")

    return access_token


def is_allowed_host(host):
    for allowed_host in ALLOWED_REDIRECT_HOSTS:
        if host == allowed_host or host.endswith('.' + allowed_host):
            return True
    print(f"Rejected redirect to unauthorized host: {host}")
    return False


def is_valid_redirect_uri(uri):
    """
    Validate that a redirect URI points to an allowed domain with a safe scheme.
    Prevents open redirect attacks where an attacker could redirect
    the JWT token to a malicious site.

    Security checks:
    - Scheme must be https (or http only for localhost/127.0.0.1)
    - Host must be in ALLOWED_REDIRECT_HOSTS
    """
    if not uri:
        return False

    try:
        parsed = urllib.parse.urlparse(uri)
        host = parsed.hostname
        scheme = parsed.scheme.lower() if parsed.scheme else ''

        if not host or not scheme:
            print("Rejected redirect with missing host or scheme")
            return False

        is_localhost = host in ('localhost', '127.0.0.1')

        # Validate scheme: only https allowed, except http for localhost
        if scheme not in ('https', 'http'):
            print(f"Rejected redirect with invalid scheme: {scheme}")
            return False

        # http is only allowed for localhost development
        if scheme == 'http' and not is_localhost:
            print(f"Rejected http redirect to non-localhost host: {host}")
            return False

        # Check against allowed hosts
        return is_allowed_host(host)

    except Exception as e:
        print(f"Error validating redirect URI: {e}")
        return False


def redirect_to_frontend(redirect_uri, parms):
    """
    Validate redirection and respond with fully build redirection url to frontend.
    """
    # Validate redirect URI to prevent open redirect attacks
    if not is_valid_redirect_uri(redirect_uri):
        raise RuntimeError('Invalid request')

    # Set base URL after validation
    base_url = redirect_uri

    # Check and build proper callback path if not provided
    if not base_url.endswith('/auth/github/callback'):
        base_url = base_url.rstrip('/') + '/auth/github/callback'

    # Build full parameterized redirect url
    redirect_url = f"{base_url}?{urllib.parse.urlencode(parms)}"

    # Return response
    return {
        'statusCode': 302,
        'headers': {
            'Location': redirect_url,
            'Cache-Control': 'no-cache, no-store, must-revalidate'
        },
        'body': ''
    }


# =============================================================================
# Device Flow handlers (for CLI/Python clients)
# =============================================================================

def handle_device_code_request(event):
    """
    Handle Device Flow initiation.
    Returns device_code, user_code, and verification_uri for the user.

    POST /auth/github/device
    """
    try:
        # Request device code from GitHub
        response = requests.post(
            GITHUB_DEVICE_CODE_URL,
            headers={
                'Accept': 'application/json',
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            data={
                'client_id': GITHUB_CLIENT_ID,
                'scope': 'read:org'
            },
            timeout=HTTP_TIMEOUT_SECONDS
        )

        if response.status_code != 200:
            print(f"GitHub device code request failed: {response.status_code} {response.text}")
            return json_response(500, {
                'error': 'device_code_request_failed',
                'error_description': f"GitHub returned status {response.status_code}"
            })

        data = response.json()

        if 'error' in data:
            return json_response(400, {
                'error': data.get('error'),
                'error_description': data.get('error_description', 'Unknown error')
            })

        # Return the device code info to the client
        return json_response(200, {
            'device_code': data.get('device_code'),
            'user_code': data.get('user_code'),
            'verification_uri': data.get('verification_uri'),
            'verification_uri_complete': data.get('verification_uri_complete'),
            'expires_in': data.get('expires_in'),
            'interval': data.get('interval', 5)
        })

    except Exception as e:
        print(f"Error in device code request: {e}")
        return json_response(500, {
            'error': 'internal_error',
            'error_description': 'error in device code request'
        })


def handle_device_poll(event):
    """
    Handle Device Flow polling.
    Client polls this endpoint with device_code to check if user has authorized.

    POST /auth/github/device/poll
    Body: { "device_code": "..." }

    Returns:
    - 200 with membership info if authorized
    - 202 with authorization_pending if user hasn't authorized yet
    - 400 with error if authorization failed or expired
    """
    try:
        # Parse request body
        body = event.get('body', '{}')
        if event.get('isBase64Encoded', False):
            import base64
            body = base64.b64decode(body).decode('utf-8')

        try:
            params = json.loads(body) if body else {}
        except json.JSONDecodeError:
            # Try URL-encoded format
            params = dict(urllib.parse.parse_qsl(body))

        device_code = params.get('device_code')

        if not device_code:
            return json_response(400, {
                'error': 'missing_device_code',
                'error_description': 'device_code is required'
            })

        # Poll GitHub for access token
        client_secret = get_github_client_secret()

        response = requests.post(
            GITHUB_TOKEN_URL,
            headers={
                'Accept': 'application/json',
                'Content-Type': 'application/x-www-form-urlencoded'
            },
            data={
                'client_id': GITHUB_CLIENT_ID,
                'client_secret': client_secret,
                'device_code': device_code,
                'grant_type': 'urn:ietf:params:oauth:grant-type:device_code'
            },
            timeout=HTTP_TIMEOUT_SECONDS
        )

        data = response.json()

        # Check for errors
        if 'error' in data:
            error = data.get('error')

            # authorization_pending means user hasn't authorized yet - keep polling
            if error == 'authorization_pending':
                return json_response(202, {
                    'status': 'pending',
                    'error': error,
                    'error_description': data.get('error_description', 'Waiting for user authorization')
                })

            # slow_down means client is polling too fast
            if error == 'slow_down':
                return json_response(202, {
                    'status': 'pending',
                    'error': error,
                    'error_description': data.get('error_description', 'Polling too fast'),
                    'interval': data.get('interval', 10)
                })

            # Other errors are terminal
            return json_response(400, {
                'status': 'error',
                'error': error,
                'error_description': data.get('error_description', 'Authorization failed')
            })

        # Success! We have an access token
        access_token = data.get('access_token')
        if not access_token:
            return json_response(500, {
                'status': 'error',
                'error': 'no_access_token',
                'error_description': 'No access token in response'
            })

        # Authenticate user to get token and metadata
        token, metadata = authenticate_user(access_token, event)

        # Response with a successful authentication
        return json_response(200, {"status": "success", "token": token, "metadata": metadata})

    except Exception as e:
        print(f"Error in device poll: {e}")
        return json_response(500, {
            'status': 'error',
            'error': 'internal_error',
            'error_description': 'error in device poll'
        })


def json_response(status_code, body):
    """Return a JSON API response with CORS headers."""
    return {
        'statusCode': status_code,
        'headers': {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*',
            'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
            'Access-Control-Allow-Headers': 'Content-Type'
        },
        'body': json.dumps(body)
    }


# =============================================================================
# Public Key / JWKS endpoints (for JWT verification)
# =============================================================================

def handle_jwks(event):
    """
    Return the public key in JWKS (JSON Web Key Set) format.

    GET /auth/github/jwks or /.well-known/jwks.json

    JWKS is the standard format for distributing public keys for JWT verification.
    This format is compatible with most JWT libraries and OIDC implementations.
    """
    # Local Utility Function
    #   Parse DER-encoded RSA SubjectPublicKeyInfo returned by KMS.get_public_key()
    #   to extract PEM, RSA modulus n, and exponent e
    def parse_rsa_public_key_spki(der_bytes):
        # parse top-level SubjectPublicKeyInfo
        spki, _ = decoder.decode(der_bytes, asn1Spec=SubjectPublicKeyInfo())
        # extract the inner RSAPublicKey DER
        rsa_der = spki['subjectPublicKey'].asOctets()
        # parse actual RSA key
        rsa_key, _ = decoder.decode(rsa_der, asn1Spec=RSAPublicKey())
        n = int(rsa_key['modulus'])
        e = int(rsa_key['publicExponent'])
        # convert SPKI to PEM
        pem = (
            "-----BEGIN PUBLIC KEY-----\n" +
            base64.encodebytes(der_bytes).decode().replace("\n", "") +
            "\n-----END PUBLIC KEY-----\n"
        )
        # return
        return {"pem": pem, "n": n, "e": e}

    # Local Utility Function
    #   Base64url-encode an unsigned big integer
    def b64url_uint(n):
        return base64.urlsafe_b64encode(n.to_bytes((n.bit_length() + 7) // 8, "big")).rstrip(b"=").decode()

    try:
        # import dependencies needed for just this function
        from pyasn1.codec.der import decoder
        from pyasn1_modules.rfc2459 import SubjectPublicKeyInfo
        from pyasn1_modules.rfc2437 import RSAPublicKey

        # check for signing key
        if not JWT_SIGNING_KEY_ARN:
            raise ValueError("JWT_SIGNING_KEY_ARN environment variable not set")

        # get public key from kms
        kms = get_kms_client()
        response = kms.get_public_key(KeyId=JWT_SIGNING_KEY_ARN)
        der = response["PublicKey"]
        parsed = parse_rsa_public_key_spki(der)
        jwk = {
            "kty": "RSA",
            "alg": "RS256",
            "use": "sig",
            "kid": response["KeyId"].split("/")[-1], # JWKS kid = key UUID
            "n": b64url_uint(parsed["n"]),
            "e": b64url_uint(parsed["e"]),
        }

        # return response
        return {
            "statusCode": 200,
            "headers": {
                "Content-Type": "application/json",
                # Recommended for browser-based OIDC discovery
                "Cache-Control": "public, max-age=3600"
            },
            "body": json.dumps({"keys": [jwk]})
        }

    except Exception as e:
        print(f"Error generating JWKS: {e}")
        return json_response(500, {
            'error': 'jwks_error',
            'error_description': 'error generating JWKS'
        })


# =============================================================================
# Lambda Function for Login
# =============================================================================

def lambda_handler(event, context):
    """
    Main Lambda handler - routes requests based on path.
    """
    path = event.get('rawPath', '')
    method = event.get('requestContext', {}).get('http', {}).get('method', 'GET')

    print(f"Received request: {method} {path}")

    # Web client Authorization Code flow
    if path == '/auth/github/login':
        return handle_login(event)
    elif path == '/auth/github/callback':
        return handle_callback(event)
    # Device Flow for CLI/Python clients
    elif path == '/auth/github/device':
        return handle_device_code_request(event)
    elif path == '/auth/github/device/poll':
        return handle_device_poll(event)
    # Public key endpoint for JWT verification
    elif path == '/auth/github/jwks' or path == '/.well-known/jwks.json':
        return handle_jwks(event)
    # Unknown path
    else:
        return {
            'statusCode': 404,
            'body': json.dumps({'error': 'Not found'})
        }
