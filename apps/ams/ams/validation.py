import os
import traceback
from pathlib import Path
from functools import wraps
from flask import request, jsonify, make_response

from openapi_core import OpenAPI
from openapi_core.contrib.flask import FlaskOpenAPIRequest, FlaskOpenAPIResponse

VALIDATE_OPENAPI = os.environ.get("VALIDATE_OPENAPI", "false").lower() in ("1", "true", "yes")

# Create OpenAPI object (from_file_path resolves $ref to external files)
if VALIDATE_OPENAPI:
    openapi = OpenAPI.from_file_path(Path("openapi/openapi.yml"))
else:
    openapi = None


def validate(func):
    if not VALIDATE_OPENAPI:
        return func

    @wraps(func)
    def wrapper(*args, **kwargs):
        # --- Validate request ---
        openapi_request = FlaskOpenAPIRequest(request)

        try:
            openapi.validate_request(openapi_request)
        except Exception as e:
            print(f"[OPENAPI] Request validation failed: {request.method} {request.path}")
            print(f"[OPENAPI]   Error: {e}")
            traceback.print_exc()
            return jsonify({
                "error": "Request validation failed",
                "details": [str(e)]
            }), 400

        # --- Call route ---
        response = func(*args, **kwargs)
        flask_response = make_response(response)
        if not flask_response.content_type or 'json' not in flask_response.content_type:
            flask_response.content_type = 'application/json'

        # --- Validate response ---
        openapi_response = FlaskOpenAPIResponse(flask_response)

        try:
            openapi.validate_response(openapi_request, openapi_response)
        except Exception as e:
            print(f"[OPENAPI] Response validation failed: {request.method} {request.path}")
            print(f"[OPENAPI]   Error: {e}")
            print(f"[OPENAPI]   Response body: {flask_response.get_data(as_text=True)[:500]}")
            traceback.print_exc()
            return jsonify({
                "error": "Response validation failed",
                "details": [str(e)]
            }), 500

        return flask_response

    return wrapper