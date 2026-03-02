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

# JWT base audiences
JWT_AUDIENCES = ['provisioner', 'runner']

# Cache for secrets (Lambda container reuse)
_secrets_cache = {}

_tmp_database = {}

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
# API Gateway Helper Functions
# =============================================================================

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

def parse_form_body(event):
    body = event.get('body') or ''
    if event.get('isBase64Encoded'):
        import base64
        body = base64.b64decode(body).decode('utf-8')
    return dict(urllib.parse.parse_qsl(body, keep_blank_values=True))


# =============================================================================
# HMAC State Helper Functions
# =============================================================================

def create_signed_state(payload=''):
    """
    Create an HMAC-signed OAuth state parameter for CSRF protection.

    The state includes:
    - A random nonce for uniqueness
    - A timestamp for expiration checking
    - Optional redirect URI
    - HMAC signature to prevent tampering

    Format: {nonce}:{timestamp}:{redirect_uri_b64}:{signature}

    Args:
        payload: Can be used for frontend redirect URI to include in state

    Returns:
        A signed state string
    """
    signing_key = get_hmac_signing_key()
    nonce = secrets.token_urlsafe(16)
    timestamp = str(int(datetime.now(timezone.utc).timestamp()))

    # Create message to sign (nonce:timestamp:redirect_uri_b64)
    message = f"{nonce}:{timestamp}:{payload}"

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
    try:
        # parse state into components
        parts = state.split(':')
        if len(parts) == 4:
            raise RuntimeError("Invalid state format")
        nonce = parts[0]
        timestamp_str = parts[1]
        payload = parts[2]
        provided_signature = parts[3]

        # Verify HMAC signature
        signing_key = get_hmac_signing_key()
        message = f"{nonce}:{timestamp_str}:{payload}"
        expected_signature = hmac.new(
            signing_key.encode(),
            message.encode(),
            hashlib.sha256
        ).hexdigest()

        if not hmac.compare_digest(provided_signature, expected_signature):
            raise RuntimeError("Invalid state signature")

        # Check timestamp expiration
        timestamp = int(timestamp_str)
        now = int(datetime.now(timezone.utc).timestamp())
        if now - timestamp > STATE_EXPIRATION_SECONDS:
            raise RuntimeError("State has expired")

        # Success
        return payload

    except Exception as e:

        # Failure
        print(f"Error verifying state: {e}")
        return None


def verify_with_kms(message_bytes, signature):
    """
    Verify a signature using KMS public key.
    Returns True if signature is valid, False otherwise
    """
    try:
        if not JWT_SIGNING_KEY_ARN:
            raise ValueError("JWT_SIGNING_KEY_ARN environment variable not set")

        kms = get_kms_client()
        response = kms.verify(
            KeyId=JWT_SIGNING_KEY_ARN,
            Message=message_bytes,
            MessageType='RAW',
            Signature=signature,
            SigningAlgorithm='RSASSA_PKCS1_V1_5_SHA_256'
        )

        return response.get('SignatureValid', False)

    except Exception as e:
        print(f"KMS verification error: {e}")
        return False


# =============================================================================
# GitHub Helper Functions
# =============================================================================

def get_github_user(authorization_str):
    """Get authenticated user's GitHub profile."""
    response = requests.get(
        f"{GITHUB_API_URL}/user",
        headers={
            'Authorization': authorization_str,
            'Accept': 'application/vnd.github.v3+json'
        },
        timeout=HTTP_TIMEOUT_SECONDS
    )

    if response.status_code != 200:
        raise Exception(f"Failed to get user info: {response.text}")

    return response.json()


def get_organization_roles(authorization_str, username):
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
            'Authorization': authorization_str,
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


def get_user_teams(authorization_str, org_roles):
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
                'Authorization': authorization_str,
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

def generate_audience_list(username, teams, org_roles):
    """
    Returns a list of cluster names the user can access and deploy.
    """
    # Initialize allowed clusters
    allowed = []

    # Early check if member
    if 'member' not in org_roles:
        return allowed

    # All members can deploy to their personal cluster
    if username:
        allowed.append(f"{username}")

    # All members can deploy to their team clusters
    if teams:
        allowed.extend(teams)

    # Owners can deploy anything
    if 'owner' in org_roles:
        allowed.extend(['*'])

    # Return allowed clusters
    return allowed


def create_auth_token(metadata):
    """
    Create a minimal signed JWT containing only server-essential fields.
    This token is validated server-side only; clients should treat it as opaque.
    """
    # Build JWT header
    header = {
        'alg': JWT_ALGORITHM,
        'typ': 'JWT',
        'kid': JWT_SIGNING_KEY_ARN.split("/")[-1] # JWKS kid = key UUID
    }

    # Helper function
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


def refresh_auth_token(token, event):
    """
    Refresh an existing JWT by extending its expiration time.
    Validates the current token and issues a new one with updated timestamps.
    """
    # Helper function
    def base64url_decode(data):
        # Add padding if needed
        padding = 4 - (len(data) % 4)
        if padding != 4:
            data += '=' * padding
        return base64.urlsafe_b64decode(data)

    try:
        # Split token into parts
        parts = token.split('.')
        if len(parts) != 3:
            raise ValueError("Invalid token format")
        header_b64, payload_b64, signature_b64 = parts

        # Decode header and payload
        header = json.loads(base64url_decode(header_b64))
        payload = json.loads(base64url_decode(payload_b64))

        # Verify token algorithm
        if header.get('alg') != JWT_ALGORITHM:
            raise ValueError(f"Invalid algorithm: expected {JWT_ALGORITHM}")

        # Verify token signature using KMS
        signing_input = f"{header_b64}.{payload_b64}"
        signature = base64url_decode(signature_b64)
        if not verify_with_kms(signing_input.encode('utf-8'), signature):
            raise ValueError("Invalid token signature")

        # Check if token is expired
        now = datetime.now(timezone.utc)
        exp = payload.get('exp')
        if not exp:
            raise ValueError("Token missing expiration")
        exp_time = datetime.fromtimestamp(exp, tz=timezone.utc)
        if now > exp_time:
            raise ValueError("Token expired and cannot be refreshed")

        # Create new metadata with updated timestamps
        metadata = payload.copy()
        new_expiration = now + timedelta(hours=JWT_EXPIRATION_HOURS)
        metadata['iat'] = int(now.timestamp())
        metadata['exp'] = int(new_expiration.timestamp())

        # Generate new token and expiration
        return create_auth_token(metadata), metadata['exp']

    except Exception as e:
        raise Exception(f"Token refresh failed: {e}")


def authenticate_user(authorization_str, event):
    """
    Build the authentication token and metadata for the user
    """
    # Get user info
    user_info = get_github_user(authorization_str)

    # Get username
    username = user_info.get('login')
    if not username:
        raise RuntimeError('Could not get GitHub username')

    # Get user metadata
    org_roles = get_organization_roles(authorization_str, username)
    teams = get_user_teams(authorization_str, org_roles)
    audience_list = generate_audience_list(username, teams, org_roles)

    # Token expiration based on JWT_EXPIRATION_HOURS config
    now = datetime.now(timezone.utc)
    expiration = now + timedelta(hours=JWT_EXPIRATION_HOURS)

    # Build issuer URL from API host (for JWKS discovery at {iss}/.well-known/jwks.json)
    headers = event.get('headers', {})
    api_host = headers.get('host', '')
    issuer = f"https://{api_host}"

    # Build metadata dictionary
    metadata = {
        'org_roles': org_roles,
        'sub': username,
        'aud': JWT_AUDIENCES + audience_list,
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
    redirect_uri_b64 = base64.urlsafe_b64encode(redirect_uri.encode()).decode()
    state = create_signed_state(redirect_uri_b64)

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
    try:
        query_params = event.get('queryStringParameters') or {}
        code = query_params.get('code')
        state = query_params.get('state')
        error = query_params.get('error')
        error_description = query_params.get('error_description')
        redirect_uri_b64 = verify_signed_state(state) # verify state parameter FIRST (CSRF protection)
        redirect_uri = base64.urlsafe_b64decode(redirect_uri_b64.encode()).decode()

        # Handle OAuth errors from GitHub
        if error:
            return redirect_to_frontend(redirect_uri, parms={'error': error_description or error})
        if not code:
            return redirect_to_frontend(redirect_uri, parms={'error': 'No authorization code received'})

        # Authenticate user (gets token and metadata)
        access_token = exchange_code_for_token(code, event)
        token, metadata = authenticate_user(f'Bearer {access_token}', event)

        # Redirect to frontend with JWT in parameters
        return redirect_to_frontend(redirect_uri, parms={
            'username': metadata['sub'],
            'isOrgMember': 'true' if ('member' in metadata["org_roles"]) else 'false',
            'isOrgOwner': 'true' if ('owner' in metadata["org_roles"]) else 'false',
            'org': metadata['org'],
            'orgRoles': ','.join(metadata['org_roles']),
            'tokenIssuedAt': str(metadata['iat']),
            'tokenExpiresAt': str(metadata['exp']),
            'tokenIssuer': metadata['iss'],
            'token': token
        })

    except Exception as e:
        print(f"Exception in OAuth callback: {e}")
        return redirect_to_frontend(redirect_uri, parms={'error': f"Error during OAuth callback"})


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


def redirect_to_frontend(redirect_uri, parms=None):
    """
    Validate redirection and respond with fully build redirection url to frontend.
    """
    # Validate redirect URI to prevent open redirect attacks
    if not is_valid_redirect_uri(redirect_uri):
        raise RuntimeError('Invalid request')

    # Build full redirect url
    redirect_url = f"{redirect_uri}?{urllib.parse.urlencode(parms)}"

    # Initialize headers
    headers = {
        'Location': redirect_url,
        'Cache-Control': 'no-cache, no-store, must-revalidate'
    }

    # Return response
    return {
        'statusCode': 302,
        'headers': headers,
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
        print(f"Response from GitHub: {data}")

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
        token, metadata = authenticate_user(f'Bearer {access_token}', event)

        # Response with a successful authentication
        return json_response(200, {"status": "success", "token": token, "metadata": metadata})

    except Exception as e:
        print(f"Error in device poll: {e}")
        return json_response(500, {
            'status': 'error',
            'error': 'internal_error',
            'error_description': 'error in device poll'
        })


# =============================================================================
# GitHub PAT key login
# =============================================================================

def handle_pat_login(event):
    """
    Handle GitHub login via Personal Access Token (PAT).
    Client sends their PAT, and we verify it and return JWT and metadata.

    POST /auth/github/pat
    Body: { "pat": "..." }
    """

    try:
        # Parse request body
        body = event.get('body', '{}')
        if event.get('isBase64Encoded', False):
            body = base64.b64decode(body).decode('utf-8')

        try:
            params = json.loads(body) if body else {}
        except json.JSONDecodeError:
            params = dict(urllib.parse.parse_qsl(body))

        pat = params.get('pat')
        if not pat:
            return json_response(400, {
                'error': 'missing_pat',
                'error_description': 'Personal Access Token (pat) is required'
            })

        # Verify token by calling GitHub API
        user_resp = requests.get(
            'https://api.github.com/user',
            headers={
                'Authorization': f'token {pat}',
                'Accept': 'application/json'
            },
            timeout=HTTP_TIMEOUT_SECONDS
        )
        if user_resp.status_code != 200:
            return json_response(400, {
                'error': 'invalid_token',
                'error_description': 'GitHub token invalid or insufficient scope'
            })

        # Token is valid! Authenticate user to get your own session token/metadata
        token, metadata = authenticate_user(f'token {pat}', event)
        return json_response(200, {'status': 'success', 'token': token, 'metadata': metadata})

    except Exception as e:
        print(f"Error in PAT login: {e}")
        return json_response(500, {
            'status': 'error',
            'error': 'internal_error',
            'error_description': 'Error processing PAT login'
        })

# =============================================================================
# Refresh tokens
# =============================================================================

def handle_refresh(event):
    """Handle JWT refresh requests"""
    try:
        # Get token from Authorization header
        auth_header = event.get('headers', {}).get('authorization', '')
        if not auth_header.startswith('Bearer '):
            raise RuntimeError('Missing or invalid authorization header')
        old_token = auth_header.replace('Bearer ', '')

        # Refresh the token
        token, expiration = refresh_auth_token(old_token, event)
        return json_response(200, {'token': token, 'exp': expiration})

    except Exception as e:
        print(f"Token refresh failed: {e}")
        return json_response(401, {
            'error': 'token_refresh_failed',
            'error_description': 'failed to refresh token'
        })

# =============================================================================
# Public Key / JWKS endpoints (for JWT verification)
# =============================================================================

# Local Utility Function
#   Parse DER-encoded RSA SubjectPublicKeyInfo returned by KMS.get_public_key()
#   to extract PEM, RSA modulus n, and exponent e
def parse_rsa_public_key_spki(der_bytes):
    # import dependencies needed for just this function
    from pyasn1.codec.der import decoder
    from pyasn1_modules.rfc2459 import SubjectPublicKeyInfo
    from pyasn1_modules.rfc2437 import RSAPublicKey
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


def handle_pem(event):
    """
    Return the public key in PEM format (needed by HAProxy).

    GET /auth/github/pem
    """
    try:

        # check for signing key
        if not JWT_SIGNING_KEY_ARN:
            raise ValueError("JWT_SIGNING_KEY_ARN environment variable not set")

        # get public key from kms
        kms = get_kms_client()
        response = kms.get_public_key(KeyId=JWT_SIGNING_KEY_ARN)
        der = response["PublicKey"]
        parsed = parse_rsa_public_key_spki(der)

        # return response
        return {
            "statusCode": 200,
            "headers": {
                "Content-Type": "application/json",
                # Recommended for browser-based OIDC discovery
                "Cache-Control": "public, max-age=3600"
            },
            "body": parsed["pem"]
        }

    except Exception as e:
        print(f"Error generating PEM: {e}")
        return json_response(500, {
            'error': 'pem_error',
            'error_description': 'error generating PEM'
        })


def handle_jwks(event):
    """
    Return the public key in JWKS (JSON Web Key Set) format.

    GET /.well-known/jwks.json

    JWKS is the standard format for distributing public keys for JWT verification.
    This format is compatible with most JWT libraries and OIDC implementations.
    """
    # base64url-encode an unsigned big integer
    def b64url_uint(n):
        return base64.urlsafe_b64encode(n.to_bytes((n.bit_length() + 7) // 8, "big")).rstrip(b"=").decode()

    try:

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
# OpenID Configuration Discovery
# =============================================================================

def handle_openid(event):
    """
    Return standard compliant discovery response
    """
    try:
        # Build issuer URL from API host (for JWKS discovery at {iss}/.well-known/jwks.json)
        headers = event.get('headers', {})
        api_host = headers.get('host', '')
        issuer = f"https://{api_host}"

        # build response body
        body = {
            "issuer": issuer,
            "jwks_uri": f"{issuer}/.well-known/jwks.json",
            "authorization_endpoint": f"{issuer}/authorize", # stub (unimplemented)
            "token_endpoint": f"{issuer}/token", # stub (unimplemented)
            "response_types_supported": ["code"],
            "subject_types_supported": ["public"],
            "id_token_signing_alg_values_supported": ["RS256"]
        }

        # return response
        return {
            "statusCode": 200,
            "headers": {
                "Content-Type": "application/json",
                # Recommended for browser-based OIDC discovery
                "Cache-Control": "public, max-age=3600"
            },
            "body": json.dumps(body)
        }

    except Exception as e:
        print(f"Error in openid discovery: {e}")
        return json_response(500, {
            'error': 'openid_error',
            'error_description': 'error in openid discovery'
        })


# =============================================================================
# OAuth 2.1 Authorization Flow (with PKCE)
# =============================================================================

def verify_code_challenge(code_verifier, code_challenge):
    digest = hashlib.sha256(code_verifier.encode('ascii')).digest()
    expected_challenge = base64.urlsafe_b64encode(digest).rstrip(b'=').decode()
    return hmac.compare_digest(expected_challenge, code_challenge)

def handle_oauth21_authorize(event):
    """
    Handle login via OAuth2.1.
    We present as a Authorization Server though we relay to GitHub
    """

    try:
        # Get query parameters
        parms                   = event.get('queryStringParameters') or {}
        response_type           = parms.get("response_type")
        client_id               = parms.get("client_id", "") # ignored
        redirect_uri            = parms.get("redirect_uri")
        client_state            = parms.get("state")
        scope                   = parms.get("scope") # ignored
        code_challenge          = parms.get("code_challenge")
        code_challenge_method   = parms.get("code_challenge")
        resource                = parms.get("resource") # ignored

        # check response type
        if response_type != "code":
            raise RuntimeError(f"Invalid response type: {response_type}")

        # check code challenge method
        if code_challenge_method != "S256":
            raise RuntimeError(f"Invalid code challenge method: {code_challenge_method}")

        # save off the code challenge
        session = secrets.token_urlsafe(32)
        _tmp_database[session] = {
            "challenge": code_challenge,
            "redirect_uri": redirect_uri
        }

        ##################
        # Relay to GitHub
        ##################

        # Build the callback URL (this Lambda's callback endpoint)
        headers = event.get('headers', {})
        host = headers.get('host', '')
        protocol = 'https'
        callback_url = f"{protocol}://{host}/authorize/callback"

        # Create HMAC-signed state for CSRF protection
        # the state includes the redirect_uri securely encoded
        # and the original state provided by the client
        redirect_uri_b64 = base64.urlsafe_b64encode(redirect_uri.encode()).decode()
        github_state = create_signed_state(f"{session}:{redirect_uri_b64}:{client_state}")

        # Build GitHub authorization URL
        github_parms = {
            'client_id': GITHUB_CLIENT_ID,
            'redirect_uri': callback_url,
            'scope': 'read:org',
            'state': github_state
        }
        auth_url = f"{GITHUB_AUTHORIZE_URL}?{urllib.parse.urlencode(github_parms)}"

        # Return response
        return {
            'statusCode': 302,
            'headers': {
                'Location': auth_url,
                'Cache-Control': 'no-cache, no-store, must-revalidate'
            },
            'body': ''
        }

    except Exception as e:
        print(f"Error in PAT login: {e}")
        return json_response(500, {
            'status': 'error',
            'error': 'internal_error',
            'error_description': 'Error processing PAT login'
        })


def handle_oauth21_callback(event):
    """
    Handle GitHub OAuth callback.
    Exchange code for token, build metadata, redirect to frontend.
    Security: Validates HMAC-signed state parameter to prevent CSRF attacks.
    """
    try:
        query_params = event.get('queryStringParameters') or {}
        code = query_params.get('code')
        github_state = query_params.get('state')
        error = query_params.get('error')
        error_description = query_params.get('error_description')
        payload_b64 = verify_signed_state(github_state) # verify state parameter FIRST (CSRF protection)
        payload = base64.urlsafe_b64decode(payload_b64.encode()).decode()
        session, redirect_uri, client_state = payload.split(":")

        # Handle OAuth errors from GitHub
        if error:
            return redirect_to_frontend(redirect_uri, parms={'error': error_description or error})
        if not code:
            return redirect_to_frontend(redirect_uri, parms={'error': 'No authorization code received'})

        # Authenticate user (gets token and metadata)
        access_token = exchange_code_for_token(code, event)
        token, metadata = authenticate_user(f'Bearer {access_token}', event)
        _tmp_database[session]["token"] = token
        _tmp_database[session]["metadata"] = metadata

        # Redirect to frontend with JWT in parameters
        return redirect_to_frontend(redirect_uri, parms={
            'code': session,
            'state': client_state
        })

    except Exception as e:
        print(f"Exception in OAuth callback: {e}")
        return redirect_to_frontend(redirect_uri, parms={'error': f"Error during OAuth callback"})


def handle_oauth21_token(event):
    """
    """
    try:
        parms           = parse_form_body(event)
        grant_type      = parms.get('grant_type')
        code            = parms.get('code')
        redirect_uri    = parms.get('redirect_uri')
        client_id       = parms.get('client_id') # ignored
        code_verifier   = parms.get('code_verifier')

        # get session information
        metadata = _tmp_database[code]["metadata"]
        token = _tmp_database[code]["token"]
        challenge = _tmp_database[code]["challenge"]
        del _tmp_database[code] # clear

        # verify grant type
        if grant_type != "authorization_code":
            raise RuntimeError(f"Invalid grant type: {grant_type}")

        # verify challenge
        if not verify_code_challenge(code_verifier, challenge):
            raise RuntimeError("Failed to verify code challenge")

        # verify redirect uri
        if _tmp_database[code]["redirect_uri"] != redirect_uri:
            raise RuntimeError(f"Mismatched redirect uri: {redirect_uri}")

        # Build known and deployable clusters (user interface hints)
        info = {
            'username': metadata['sub'],
            'isOrgMember': 'true' if ('member' in metadata["org_roles"]) else 'false',
            'isOrgOwner': 'true' if ('owner' in metadata["org_roles"]) else 'false',
            'org': metadata['org'],
            'orgRoles': ','.join(metadata['org_roles']),
            'tokenIssuedAt': str(metadata['iat']),
            'tokenExpiresAt': str(metadata['exp']),
            'tokenIssuer': metadata['iss']
        }

        # Redirect to frontend with JWT in parameters
        return json_response(200, {
            "access_token": token,
            "token_type": "Bearer",
            "expires_in": JWT_EXPIRATION_HOURS * 60,
            "refresh_token": token, # revisit
            "scope": "mcp:tools mcp:resources",
            "info": info
        })

    except Exception as e:
        print(f"Exception in OAuth token: {e}")
        return json_response(500, {
            'error': 'token_error',
            'error_description': 'error in token'
        })


def handle_oauth21_server(event: dict) -> dict:
    """
    Serve OAuth 2.0 Authorization Server Metadata per RFC 8414.
    Endpoint: GET /.well-known/oauth-authorization-server
    """
    host = event.get('headers', {}).get('host', '')
    base_url = f"https://{host}"
    metadata = {
        "issuer": base_url, # Validates that the metadata document it  received came from the expected AS (prevents AS mix-up attacks).
        "authorization_endpoint": f"{base_url}/oauth/authorize", # Used by client as log in destination. Required for any AS that supports authorization_code grant.
        "token_endpoint": f"{base_url}/oauth/token", # Used by client for POSTs to exchange a code for a token, and later to refresh an expired token.
        "response_types_supported": ["code"], # Required — the response types this AS can produce. Only "code" for OAuth 2.1 (implicit/"token" is removed).
        "scopes_supported": ["mcp:tools", "mcp:resources", "offline_access"],
        "token_endpoint_auth_methods_supported": ["none"], # "none" means no client_secret — authentication is handled by PKCE instead
        "code_challenge_methods_supported": ["S256"], # S256 only — "plain" is removed in OAuth 2.1
        "registration_endpoint": f"{base_url}/oauth/register", # Dynamic client registration (RFC 7591)
        "revocation_endpoint": f"{base_url}/oauth/revoke", # invalidate tokens on logout
        "jwks_uri": f"{base_url}/.well-known/jwks.json", # fetching public key to verify JWT signatures
        "id_token_signing_alg_values_supported": ["RS256"], # The signing algorithms used when issuing JWTs.
        "grant_types_supported": ["authorization_code", "refresh_token"], # allows silent token renewal
    }

    return {
        "statusCode": 200,
        "headers": {
            "Content-Type": "application/json",
            # This document changes rarely — caching for 1 hour is reasonable.
            # Don't cache indefinitely in case you rotate keys or add endpoints.
            "Cache-Control": "public, max-age=3600",
        },
        "body": json.dumps(metadata, indent=2),
    }

# =============================================================================
# Lambda Function for Login
# =============================================================================

def lambda_gateway(event, context):
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
    # PAT Key login
    elif path == '/auth/github/pat':
        return handle_pat_login(event)
    # Refresh token
    elif path == '/auth/refresh':
        return handle_refresh(event)
    # Public key endpoint for JWT verification (PEM)
    elif path == '/auth/github/pem':
        return handle_pem(event)
    # Public key endpoint for JWT verification (JWKS)
    elif path == '/.well-known/jwks.json':
        return handle_jwks(event)
    # OpenID Configuration Discovery
    elif path == '/.well-known/openid-configuration':
        return handle_openid(event)
    # OAuth 2.1 Service Metadata
    elif path == '/.well-known/oauth-authorization-server':
        return handle_oauth21_server(event)
    # Unknown path
    else:
        return {
            'statusCode': 404,
            'body': json.dumps({'error': 'Not found'})
        }
