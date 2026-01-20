"""
GitHub Organization Authentication for SlideRule Python Client

This module provides Device Flow authentication to verify membership
in the SlideRuleEarth GitHub organization.
"""

import json
import time
import webbrowser
from typing import Optional, TypedDict
from urllib.parse import urljoin

import requests


class AuthResult(TypedDict):
    """Result of GitHub authentication."""
    success: bool
    username: Optional[str]
    error: Optional[str]


class GitHubOrgAuth:
    """
    GitHub Organization Authentication using Device Flow.

    This allows CLI/desktop applications to authenticate users with GitHub
    and verify their membership in the SlideRuleEarth organization.
    """

    # Default API URL (test environment)
    DEFAULT_API_URL = "https://login.slideruleearth.io"

    def __init__(self, api_url: Optional[str] = None, auto_open_browser: bool = True):
        """
        Initialize the GitHub auth client.

        Args:
            api_url: The API Gateway URL for the OAuth Lambda.
                     Defaults to the test environment.
            auto_open_browser: Whether to automatically open the browser
                              for user authorization.
        """
        self.api_url = (api_url or self.DEFAULT_API_URL).rstrip('/')
        self.auto_open_browser = auto_open_browser
        self._session = requests.Session()

    def authenticate(self, timeout: int = 300) -> AuthResult:
        """
        Perform GitHub Device Flow authentication.

        This method will:
        1. Request a device code from the API
        2. Display instructions for the user to authorize
        3. Optionally open the browser for authorization
        4. Poll for the result until authorized or timeout

        Args:
            timeout: Maximum time to wait for authorization (seconds).
                    Default is 5 minutes.

        Returns:
            AuthResult with authentication status and membership info.
        """
        # Step 1: Request device code
        device_info = self._request_device_code()
        print(device_info)
        if 'error' in device_info:
            return self._error_result(device_info.get('error_description', device_info['error']))

        user_code = device_info['user_code']
        device_code = device_info['device_code']
        verification_uri = device_info.get('verification_uri_complete') or device_info['verification_uri']
        interval = device_info.get('interval', 5)
        expires_in = device_info.get('expires_in', timeout)

        # Step 2: Display instructions to user
        print("\n" + "=" * 60)
        print("GitHub Authentication Required")
        print("=" * 60)
        print(f"\nPlease visit: {verification_uri}")
        print(f"\nAnd enter this code: {user_code}")
        print("\n" + "=" * 60)

        # Step 3: Open browser if requested
        if self.auto_open_browser:
            print("\nOpening browser...")
            try:
                webbrowser.open(verification_uri)
            except Exception:
                print("Could not open browser automatically.")
                print(f"Please manually visit: {verification_uri}")

        print("\nWaiting for authorization...\n")

        # Step 4: Poll for authorization
        start_time = time.time()
        actual_timeout = min(timeout, expires_in)

        while time.time() - start_time < actual_timeout:
            result = self._poll_for_token(device_code)

            if result.get('status') == 'success':
                print(f"\nAuthentication successful!")
                print(f"Logged in as: {result['metadata']['username']}")

                return AuthResult(
                    success=True,
                    username=result['metadata']['username'],
                    error=None
                )

            elif result.get('status') == 'pending':
                # Still waiting for user authorization
                wait_interval = result.get('interval', interval)
                time.sleep(wait_interval)

            elif result.get('status') == 'error':
                return self._error_result(result.get('error_description', result.get('error')))

            else:
                # Unexpected response
                time.sleep(interval)

        return self._error_result("Authorization timed out")

    def _request_device_code(self) -> dict:
        """Request a device code from the API."""
        url = urljoin(self.api_url + '/', 'auth/github/device')
        try:
            response = self._session.post(url)
            return response.json()
        except requests.RequestException as e:
            return {'error': 'request_failed', 'error_description': str(e)}
        except json.JSONDecodeError:
            return {'error': 'invalid_response', 'error_description': 'Invalid JSON response'}

    def _poll_for_token(self, device_code: str) -> dict:
        """Poll the API to check if user has authorized."""
        url = urljoin(self.api_url + '/', 'auth/github/device/poll')
        try:
            response = self._session.post(
                url,
                json={'device_code': device_code},
                headers={'Content-Type': 'application/json'}
            )
            return response.json()
        except requests.RequestException as e:
            return {'status': 'error', 'error': 'request_failed', 'error_description': str(e)}
        except json.JSONDecodeError:
            return {'status': 'error', 'error': 'invalid_response', 'error_description': 'Invalid JSON response'}

    def _error_result(self, error_message: str) -> AuthResult:
        """Create an error result."""
        return AuthResult(
            success=False,
            username=None,
            error=error_message
        )


def main():
    """CLI entry point for testing authentication."""
    import argparse

    parser = argparse.ArgumentParser(
        description='Authenticate with GitHub and verify SlideRuleEarth membership'
    )
    parser.add_argument(
        '--api-url',
        default=GitHubOrgAuth.DEFAULT_API_URL,
        help='API Gateway URL for the OAuth Lambda'
    )
    parser.add_argument(
        '--no-browser',
        action='store_true',
        help='Do not automatically open the browser'
    )
    parser.add_argument(
        '--timeout',
        type=int,
        default=300,
        help='Timeout in seconds for authorization (default: 300)'
    )

    args = parser.parse_args()

    auth = GitHubOrgAuth(
        api_url=args.api_url,
        auto_open_browser=not args.no_browser
    )

    result = auth.authenticate(timeout=args.timeout)

    if result['success']:
        print(f"\nnAuthentication succeeded: {result['username']}")
    else:
        print(f"\nAuthentication failed: {result['error']}")
        exit(1)


if __name__ == '__main__':
    main()
