"""
GitHub OAuth Lambda Handler for SlideRule Web Client

Handles OAuth authentication flow and organization membership verification
for the SlideRuleEarth GitHub organization.

Supports two flows:
1. Authorization Code Flow (for web clients)
2. Device Flow (for CLI/Python clients)

Returns:
1. A minimal signed JWT containing only server-essential fields:
   - sub/username: GitHub username (subject)
   - accessible_clusters: list of cluster names the user can access
   - deployable_clusters: list of cluster names the user can deploy/provision
   - max_nodes: maximum compute nodes (15 for owners, 7 for members)
   - cluster_ttl_hours: max cluster runtime in hours (12 for owners, 8 for members)
   - iat: issued at timestamp
   - exp: token expiration timestamp (default 12 hours, configurable via JWT_EXPIRATION_HOURS)
   - iss: token issuer

2. User info returned separately (via URL params for web, JSON for device flow):
   - username, is_org_member, is_org_owner
   - teams, team_roles, org_roles
   - accessible_clusters, deployable_clusters, max_nodes, cluster_ttl_hours
   - org, token_issued_at, token_expires_at, token_issuer

The JWT is validated server-side only. Clients should treat it as opaque and use
the separately returned user info for display/UX purposes.
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
# Note: PyJWT is available for verification if needed, but we use KMS directly for signing

# Configuration from environment variables
GITHUB_CLIENT_ID = os.environ.get('GITHUB_CLIENT_ID')
GITHUB_ORG = os.environ.get('GITHUB_ORG', 'SlideRuleEarth')
FRONTEND_URL = os.environ.get('FRONTEND_URL', 'https://testsliderule.org')
CLIENT_SECRET_NAME = os.environ.get('CLIENT_SECRET_NAME')
# KMS key ARN for JWT signing (RS256 asymmetric)
JWT_SIGNING_KEY_ARN = os.environ.get('JWT_SIGNING_KEY_ARN')
# Secrets Manager ARN for HMAC key (OAuth state signing)
HMAC_SIGNING_KEY_ARN = os.environ.get('HMAC_SIGNING_KEY_ARN')

# JWT configuration
# RS256 (asymmetric) is preferred over HS256 (symmetric) because:
# - Private key never leaves KMS (more secure)
# - Public key can be freely distributed for verification
# - Better for distributed systems where multiple services verify tokens
JWT_ALGORITHM = 'RS256'
JWT_EXPIRATION_HOURS = int(os.environ.get('JWT_EXPIRATION_HOURS', '12'))
# JWT audience claim - services validating the token should check this
JWT_AUDIENCE = os.environ.get('JWT_AUDIENCE', 'sliderule')

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

# Allowed redirect domains (for open redirect protection)
# These are validated against the redirect_uri to prevent attackers from
# redirecting tokens to malicious sites
ALLOWED_REDIRECT_HOSTS = [
    'testsliderule.org',
    'client.slideruleearth.io',
    'localhost',
    '127.0.0.1',
]

# Cache for secrets (Lambda container reuse)
_secrets_cache = {}


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


def get_kms_public_key():
    """
    Retrieve the public key from KMS for JWT verification.

    This can be distributed freely to any service that needs to verify JWTs.
    The public key is cached for Lambda container reuse.

    Returns:
        PEM-encoded public key string
    """
    if 'kms_public_key' not in _secrets_cache:
        if not JWT_SIGNING_KEY_ARN:
            raise ValueError("JWT_SIGNING_KEY_ARN environment variable not set")

        kms = get_kms_client()
        response = kms.get_public_key(KeyId=JWT_SIGNING_KEY_ARN)

        # Convert DER to PEM format
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.backends import default_backend

        public_key = serialization.load_der_public_key(
            response['PublicKey'],
            backend=default_backend()
        )
        pem_key = public_key.public_bytes(
            encoding=serialization.Encoding.PEM,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )
        _secrets_cache['kms_public_key'] = pem_key.decode('utf-8')

    return _secrets_cache['kms_public_key']


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


def is_valid_redirect_uri(uri):
    """
    Validate that a redirect URI points to an allowed domain with a safe scheme.
    Prevents open redirect attacks where an attacker could redirect
    the JWT token to a malicious site.

    Security checks:
    - Scheme must be https (or http only for localhost/127.0.0.1)
    - Host must be in ALLOWED_REDIRECT_HOSTS or match FRONTEND_URL

    Args:
        uri: The redirect URI to validate

    Returns:
        True if the URI is safe to redirect to, False otherwise
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
        for allowed_host in ALLOWED_REDIRECT_HOSTS:
            if host == allowed_host or host.endswith('.' + allowed_host):
                return True

        # Also allow the configured FRONTEND_URL host
        frontend_parsed = urllib.parse.urlparse(FRONTEND_URL)
        if host == frontend_parsed.hostname:
            return True

        print(f"Rejected redirect to unauthorized host: {host}")
        return False

    except Exception as e:
        print(f"Error validating redirect URI: {e}")
        return False


def lambda_handler(event, context):
    """Main Lambda handler - routes requests based on path."""
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
    else:
        return {
            'statusCode': 404,
            'body': json.dumps({'error': 'Not found'})
        }


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
    Exchange code for token, check org membership, get teams, create JWT, redirect to frontend.

    Security: Validates HMAC-signed state parameter to prevent CSRF attacks.
    """
    query_params = event.get('queryStringParameters') or {}
    code = query_params.get('code')
    state = query_params.get('state', '')
    error = query_params.get('error')
    error_description = query_params.get('error_description')

    # Verify state parameter FIRST (CSRF protection)
    # This must happen before any other processing
    is_valid, redirect_uri, state_error = verify_signed_state(state)
    if not is_valid:
        print(f"Security: Invalid OAuth state - {state_error}")
        # Return error to default frontend since we can't trust the redirect_uri
        return redirect_to_frontend_with_error(
            redirect_uri=None,
            error_message=f"Security error: {state_error}"
        )

    # Handle OAuth errors from GitHub
    if error:
        return redirect_to_frontend_with_error(redirect_uri, error_description or error)

    if not code:
        return redirect_to_frontend_with_error(redirect_uri, 'No authorization code received')

    try:
        # Exchange code for access token
        access_token = exchange_code_for_token(code, event)

        # Get user info
        user_info = get_github_user(access_token)
        username = user_info.get('login')

        if not username:
            return redirect_to_frontend_with_error(redirect_uri, 'Could not get GitHub username')

        # Check organization membership
        membership = check_org_membership(access_token, username)

        # Get user's teams in the organization (only if they're a member)
        teams = []
        team_roles = {}
        if membership['is_org_member']:
            teams, team_roles = get_user_teams(access_token, username)
            print(f"User {username} belongs to teams: {teams}, team_roles: {team_roles}")

        # Compute org-level roles and accessible/deployable clusters
        org_roles = compute_org_roles(membership['is_org_member'], membership['is_org_owner'])
        accessible_clusters = compute_accessible_clusters(username, teams, membership['is_org_member'])
        deployable_clusters = compute_deployable_clusters(
            username, teams, membership['is_org_member'], membership['is_org_owner']
        )
        print(f"User {username} org_roles: {org_roles}, accessible_clusters: {accessible_clusters}, deployable_clusters: {deployable_clusters}")

        # Compute token metadata (max_nodes, cluster_ttl, timestamps)
        token_metadata = compute_token_metadata(membership['is_org_member'], membership['is_org_owner'])

        # Get API host for issuer URL (enables JWKS discovery at {iss}/.well-known/jwks.json)
        headers = event.get('headers', {})
        api_host = headers.get('host', '')

        # Create minimal signed JWT token (only server-essential fields)
        auth_token = create_auth_token(
            username=username,
            accessible_clusters=accessible_clusters,
            deployable_clusters=deployable_clusters,
            token_metadata=token_metadata,
            is_org_member=membership['is_org_member'],
            is_org_owner=membership['is_org_owner'],
            api_host=api_host
        )

        # Redirect to frontend with results (user info + token metadata returned separately)
        return redirect_to_frontend_with_result(
            redirect_uri=redirect_uri,
            username=username,
            is_org_member=membership['is_org_member'],
            is_org_owner=membership['is_org_owner'],
            teams=teams,
            team_roles=team_roles,
            org_roles=org_roles,
            accessible_clusters=accessible_clusters,
            deployable_clusters=deployable_clusters,
            token=auth_token,
            token_metadata=token_metadata
        )

    except Exception as e:
        print(f"Error during OAuth callback: {str(e)}")
        return redirect_to_frontend_with_error(redirect_uri, str(e))


def exchange_code_for_token(code, event):
    """Exchange authorization code for access token."""
    headers = event.get('headers', {})
    host = headers.get('host', '')
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


def check_org_membership(access_token, username):
    """
    Check if user is a member of the SlideRuleEarth organization.

    Returns dict with is_org_member and is_org_owner flags.

    Raises:
        Exception: If GitHub API returns an unexpected error (5xx, 429, etc.)
                   This prevents silently degrading users to non-member status
                   during GitHub outages.
    """
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

        return {
            'is_org_member': is_org_member,
            'is_org_owner': is_org_owner
        }
    elif response.status_code == 404:
        # User is not a member of the organization - this is expected for non-members
        return {
            'is_org_member': False,
            'is_org_owner': False
        }
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


def get_user_teams(access_token, username):
    """
    Get all teams the user belongs to in the SlideRuleEarth organization.

    Uses the /user/teams endpoint which works for all authenticated users,
    unlike /orgs/{org}/teams which requires admin permissions.

    Returns a tuple of (teams, team_roles) where:
    - teams: list of team slugs
    - team_roles: dict mapping team slug to role ('member' or 'maintainer')
    """
    teams = []
    team_roles = {}

    # Use /user/teams which lists teams for the authenticated user
    # This works for all users, unlike /orgs/{org}/teams which requires admin
    url = f"{GITHUB_API_URL}/user/teams"
    page = 1
    per_page = 100

    while True:
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

        if response.status_code != 200:
            print(f"Failed to get user teams: {response.status_code} {response.text}")
            break

        user_teams = response.json()
        if not user_teams:
            break

        # Filter teams by organization and get role for each
        for team in user_teams:
            org = team.get('organization', {})
            if org.get('login') == GITHUB_ORG:
                team_slug = team.get('slug')
                if team_slug:
                    # /user/teams doesn't include the user's role in the team
                    # Need to call the team membership API to get role
                    membership = get_user_team_membership(access_token, team_slug, username)
                    if membership:
                        teams.append(team_slug)
                        team_roles[team_slug] = membership.get('role', 'member')

        # Check if there are more pages
        if len(user_teams) < per_page:
            break
        page += 1

    return teams, team_roles


def get_user_team_membership(access_token, team_slug, username):
    """
    Get a user's membership details in a specific team.
    Returns None if not a member, or a dict with role info if member.
    """
    response = requests.get(
        f"{GITHUB_API_URL}/orgs/{GITHUB_ORG}/teams/{team_slug}/memberships/{username}",
        headers={
            'Authorization': f'Bearer {access_token}',
            'Accept': 'application/vnd.github.v3+json'
        },
        timeout=HTTP_TIMEOUT_SECONDS
    )

    if response.status_code == 200:
        data = response.json()
        # User must have 'active' state
        if data.get('state') == 'active':
            # role can be 'member' or 'maintainer'
            return {
                'role': data.get('role', 'member')
            }

    return None


def is_user_in_team(access_token, team_slug, username):
    """Check if a user is a member of a specific team."""
    membership = get_user_team_membership(access_token, team_slug, username)
    return membership is not None


def compute_org_roles(is_org_member, is_org_owner):
    """
    Compute org-level roles based on membership status.
    Returns a list of role strings.
    """
    roles = []
    if is_org_owner:
        roles = ['owner', 'member']
    elif is_org_member:
        roles = ['member']
    return roles


def format_team_roles(team_roles):
    """
    Format team roles into the <team>-roles structure.
    Input: {'frontend-team': 'maintainer', 'data-team': 'member'}
    Output: {'frontend-team-roles': ['maintainer'], 'data-team-roles': ['member']}
    """
    formatted = {}
    for team, role in team_roles.items():
        formatted[f"{team}-roles"] = [role]
    return formatted


def compute_accessible_clusters(username, teams, is_org_member):
    """
    Compute accessible clusters for a user.
    - 'sliderule' public cluster is always included first
    - Org members get a personal cluster with -cluster suffix (username-cluster)
    - Teams are used directly as cluster names (no suffix)

    Returns a list of cluster names the user can access.
    """
    accessible_clusters = ['sliderule']  # Public cluster always available
    if is_org_member and username:
        accessible_clusters.append(f"{username}-cluster")
    if teams:
        accessible_clusters.extend(teams)
    return accessible_clusters


def compute_deployable_clusters(username, teams, is_org_member, is_org_owner):
    """
    Compute clusters that the user can deploy/provision.

    - Non-members: empty list (no deployment rights)
    - Org owners: ['sliderule-green', 'sliderule-blue', '<username>-cluster', ...teams]
    - Org members: ['<username>-cluster', ...teams]

    Returns a list of cluster names the user can deploy.
    """
    if not is_org_member:
        return []

    deployable = []

    if is_org_owner:
        # Owners can deploy to production clusters
        deployable.extend(['sliderule-green', 'sliderule-blue'])

    # All members can deploy to their personal cluster
    if username:
        deployable.append(f"{username}-cluster")

    # All members can deploy to their team clusters
    if teams:
        deployable.extend(teams)

    return deployable


def compute_token_metadata(is_org_member, is_org_owner):
    """
    Compute token metadata (max_nodes, cluster_ttl, timestamps) based on user role.

    Returns a dict with:
    - max_nodes: maximum compute nodes
    - cluster_ttl_hours: max cluster runtime in hours
    - iat: issued at timestamp (int)
    - exp: expiration timestamp (int)
    - iss: issuer string
    """
    now = datetime.now(timezone.utc)

    # Determine max_nodes and cluster TTL based on role
    if is_org_owner:
        max_nodes = 15
        cluster_ttl_hours = 12
    elif is_org_member:
        max_nodes = 7
        cluster_ttl_hours = 8
    else:
        # Non-members get no access
        max_nodes = 0
        cluster_ttl_hours = 0

    # Token expiration based on JWT_EXPIRATION_HOURS config
    expiration = now + timedelta(hours=JWT_EXPIRATION_HOURS)

    return {
        'max_nodes': max_nodes,
        'cluster_ttl_hours': cluster_ttl_hours,
        'iat': int(now.timestamp()),
        'exp': int(expiration.timestamp()),
        'iss': 'sliderule-github-oauth'
    }


def create_auth_token(username, accessible_clusters, deployable_clusters, token_metadata,
                      is_org_member, is_org_owner, api_host=None):
    """
    Create a minimal signed JWT containing only server-essential fields.
    This token is validated server-side only; clients should treat it as opaque.

    Uses RS256 (RSA + SHA-256) with AWS KMS for signing:
    - Private key never leaves KMS (hardware security module)
    - Public key can be freely distributed for verification
    - More secure than HS256 for distributed systems

    Token includes:
    - sub/username: GitHub username (subject)
    - aud: audience claim for token validation
    - iss: issuer URL (API Gateway endpoint for JWKS discovery)
    - org: GitHub organization name
    - org_member: boolean indicating org membership
    - org_owner: boolean indicating org owner status
    - accessible_clusters: list of cluster names the user can access
    - deployable_clusters: list of cluster names the user can deploy/provision
    - max_nodes: maximum compute nodes
    - cluster_ttl_hours: max cluster runtime in hours
    - iat: issued at timestamp
    - exp: expiration timestamp

    All other user info (teams, roles, etc.) is returned
    separately to the client for UX purposes.

    Args:
        username: GitHub username
        accessible_clusters: List of cluster names user can access
        deployable_clusters: List of cluster names user can deploy
        token_metadata: Dict with max_nodes, cluster_ttl_hours, iat, exp
        is_org_member: Boolean indicating if user is org member
        is_org_owner: Boolean indicating if user is org owner
        api_host: Optional API Gateway host for issuer URL
    """
    # Build issuer URL from API host (for JWKS discovery at {iss}/.well-known/jwks.json)
    if api_host:
        issuer = f"https://{api_host}"
    else:
        issuer = token_metadata['iss']

    # Build payload with server-essential fields + API Gateway compatible claims
    payload = {
        'sub': username,  # Subject (GitHub username)
        'username': username,
        'aud': JWT_AUDIENCE,  # Audience claim for API Gateway JWT authorizer
        'iss': issuer,  # Issuer URL for JWKS discovery
        'org': GITHUB_ORG,  # Organization name
        'org_member': is_org_member,  # For authorization decisions
        'org_owner': is_org_owner,  # For elevated permission checks
        'accessible_clusters': accessible_clusters,
        'deployable_clusters': deployable_clusters,
        'max_nodes': token_metadata['max_nodes'],
        'cluster_ttl_hours': token_metadata['cluster_ttl_hours'],
        'iat': token_metadata['iat'],
        'exp': token_metadata['exp'],
    }

    # Build JWT header
    header = {
        'alg': JWT_ALGORITHM,
        'typ': 'JWT'
    }

    # Encode header and payload (base64url without padding)
    def base64url_encode(data):
        return base64.urlsafe_b64encode(data).rstrip(b'=').decode('ascii')

    header_b64 = base64url_encode(json.dumps(header, separators=(',', ':')).encode('utf-8'))
    payload_b64 = base64url_encode(json.dumps(payload, separators=(',', ':')).encode('utf-8'))

    # Create signing input
    signing_input = f"{header_b64}.{payload_b64}"

    # Sign with KMS (RS256 = RSASSA-PKCS1-v1_5 using SHA-256)
    signature = sign_with_kms(signing_input.encode('utf-8'))
    signature_b64 = base64url_encode(signature)

    # Assemble final JWT
    token = f"{signing_input}.{signature_b64}"
    return token


def redirect_to_frontend_with_result(redirect_uri, username, is_org_member, is_org_owner, teams=None, team_roles=None, org_roles=None, accessible_clusters=None, deployable_clusters=None, token=None, token_metadata=None):
    """
    Redirect to frontend with successful authentication result.

    The JWT token is minimal (server-essential fields only). All user info
    and token metadata are returned separately in URL params for client UX.

    Args:
        redirect_uri: The validated redirect URI from the signed state, or None to use default
        username: GitHub username
        is_org_member: Whether user is an org member
        is_org_owner: Whether user is an org owner
        teams: List of team slugs
        team_roles: Dict of team slug to role
        org_roles: List of org-level roles
        accessible_clusters: List of clusters the user can access
        deployable_clusters: List of clusters the user can deploy/provision
        token: JWT token (opaque to client)
        token_metadata: Dict with max_nodes, cluster_ttl_hours, iat, exp, iss
    """
    # Validate redirect URI to prevent open redirect attacks
    # Only use redirect_uri if it points to an allowed domain
    if redirect_uri and not is_valid_redirect_uri(redirect_uri):
        print(f"Security: Rejected invalid redirect URI: {redirect_uri}")
        redirect_uri = None

    # Build redirect URL (fall back to configured FRONTEND_URL if invalid)
    base_url = redirect_uri or FRONTEND_URL

    # Ensure we redirect to the callback path on the frontend
    if not base_url.endswith('/auth/github/callback'):
        base_url = base_url.rstrip('/') + '/auth/github/callback'

    params = {
        'username': username,
        'isOrgMember': 'true' if is_org_member else 'false',
        'isOrgOwner': 'true' if is_org_owner else 'false',
        'org': GITHUB_ORG
    }

    # Add teams as comma-separated list
    if teams:
        params['teams'] = ','.join(teams)

    # Add org-level roles as comma-separated list
    if org_roles:
        params['orgRoles'] = ','.join(org_roles)

    # Add team-level roles as JSON (more complex structure)
    if team_roles:
        params['teamRoles'] = json.dumps(team_roles)

    # Add accessible clusters as comma-separated list
    if accessible_clusters:
        params['accessibleClusters'] = ','.join(accessible_clusters)

    # Add deployable clusters as comma-separated list
    if deployable_clusters:
        params['deployableClusters'] = ','.join(deployable_clusters)

    # Add token metadata (for client UX display)
    if token_metadata:
        params['maxNodes'] = str(token_metadata['max_nodes'])
        params['clusterTtlHours'] = str(token_metadata['cluster_ttl_hours'])
        params['tokenIssuedAt'] = str(token_metadata['iat'])
        params['tokenExpiresAt'] = str(token_metadata['exp'])
        params['tokenIssuer'] = token_metadata['iss']

    # Add JWT token (opaque to client - used only for server auth)
    if token:
        params['token'] = token

    redirect_url = f"{base_url}?{urllib.parse.urlencode(params)}"

    return {
        'statusCode': 302,
        'headers': {
            'Location': redirect_url,
            'Cache-Control': 'no-cache, no-store, must-revalidate'
        },
        'body': ''
    }


def redirect_to_frontend_with_error(redirect_uri, error_message):
    """
    Redirect to frontend with error.

    Args:
        redirect_uri: The validated redirect URI from the signed state, or None to use default
        error_message: Error message to display
    """
    # Validate redirect URI to prevent open redirect attacks
    if redirect_uri and not is_valid_redirect_uri(redirect_uri):
        print(f"Security: Rejected invalid redirect URI in error flow: {redirect_uri}")
        redirect_uri = None

    base_url = redirect_uri or FRONTEND_URL

    if not base_url.endswith('/auth/github/callback'):
        base_url = base_url.rstrip('/') + '/auth/github/callback'

    params = {
        'error': error_message
    }

    redirect_url = f"{base_url}?{urllib.parse.urlencode(params)}"

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
        print(f"Error in device code request: {str(e)}")
        return json_response(500, {
            'error': 'internal_error',
            'error_description': str(e)
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

        # Get user info
        user_info = get_github_user(access_token)
        username = user_info.get('login')

        if not username:
            return json_response(500, {
                'status': 'error',
                'error': 'no_username',
                'error_description': 'Could not get GitHub username'
            })

        # Check organization membership
        membership = check_org_membership(access_token, username)

        # Get user's teams in the organization (only if they're a member)
        teams = []
        team_roles = {}
        if membership['is_org_member']:
            teams, team_roles = get_user_teams(access_token, username)
            print(f"User {username} belongs to teams: {teams}, team_roles: {team_roles}")

        # Compute org-level roles and accessible/deployable clusters
        org_roles = compute_org_roles(membership['is_org_member'], membership['is_org_owner'])
        accessible_clusters = compute_accessible_clusters(username, teams, membership['is_org_member'])
        deployable_clusters = compute_deployable_clusters(
            username, teams, membership['is_org_member'], membership['is_org_owner']
        )
        print(f"User {username} org_roles: {org_roles}, accessible_clusters: {accessible_clusters}, deployable_clusters: {deployable_clusters}")

        # Compute token metadata (max_nodes, cluster_ttl, timestamps)
        token_metadata = compute_token_metadata(membership['is_org_member'], membership['is_org_owner'])

        # Get API host for issuer URL (enables JWKS discovery at {iss}/.well-known/jwks.json)
        headers = event.get('headers', {})
        api_host = headers.get('host', '')

        # Create minimal signed JWT token (only server-essential fields)
        auth_token = create_auth_token(
            username=username,
            accessible_clusters=accessible_clusters,
            deployable_clusters=deployable_clusters,
            token_metadata=token_metadata,
            is_org_member=membership['is_org_member'],
            is_org_owner=membership['is_org_owner'],
            api_host=api_host
        )

        # Build response with user info and token metadata (JWT is opaque to client)
        response_data = {
            'status': 'success',
            'username': username,
            'is_org_member': membership['is_org_member'],
            'is_org_owner': membership['is_org_owner'],
            'teams': teams,
            'team_roles': team_roles,
            'org_roles': org_roles,
            'accessible_clusters': accessible_clusters,
            'deployable_clusters': deployable_clusters,
            'token': auth_token,
            'organization': GITHUB_ORG,
            # Token metadata for client UX display
            'max_nodes': token_metadata['max_nodes'],
            'cluster_ttl_hours': token_metadata['cluster_ttl_hours'],
            'token_issued_at': token_metadata['iat'],
            'token_expires_at': token_metadata['exp'],
            'token_issuer': token_metadata['iss']
        }

        return json_response(200, response_data)

    except Exception as e:
        print(f"Error in device poll: {str(e)}")
        return json_response(500, {
            'status': 'error',
            'error': 'internal_error',
            'error_description': str(e)
        })


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
    #
    # Local Utility Function
    #   Parse DER-encoded RSA SubjectPublicKeyInfo returned by KMS.get_public_key()
    #   to extract PEM, RSA modulus n, and exponent e
    #
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

    #
    # Local Utility Function
    #   Base64url-encode an unsigned big integer
    #
    def b64url_uint(n):
        return base64.urlsafe_b64encode(n.to_bytes((n.bit_length() + 7) // 8, "big")).rstrip(b"=").decode()

    #
    # Start of Function Definition
    #
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
            'error_description': str(e)
        })
