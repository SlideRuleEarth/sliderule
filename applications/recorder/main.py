import os
import boto3
import botocore.exceptions
import json
import base64
from datetime import datetime, timedelta
import scheduler

# ###############################
# Globals
# ###############################

MIN_TTL_FOR_AUTOSHUTDOWN = 15 # minutes
SYSTEM_KEYWORDS = ['login','provisioner','client']

# ###############################
# Functions
# ###############################

def report(body):
    # initialize response state
    state = {"status": True}
    # return state
    return state

# ###############################
# Lambda: Gateway Handler
# ###############################

def lambda_gateway(event, context):
    """
    Route requests based on path
    """
    # get JWT claims (validated by API Gateway)
    claims = event["requestContext"]["authorizer"]["jwt"]["claims"]
    username = claims.get('sub', '<anonymous>')
    org_roles = claims.get('org_roles', [])

    # get path and body of request
    path = event.get('rawPath', '')
    body_raw = event.get("body")
    if body_raw:
        if event.get("isBase64Encoded"):
            body_raw = base64.b64decode(body_raw).decode("utf-8")
        body = json.loads(body_raw)
    else:
        body = {}
    print(f'Received request: {path} {body}')

    # check organization ownership
    if 'owner' not in org_roles:
        print(f'Access denied to {username}, organization roles: {org_roles}')
        return {
            'statusCode': 403,
            'body': json.dumps({'error': 'access denied'})
        }

    # route request
    if path == '/report':
        return report(body)
    else:
        print(f'Path not found: {path}')
        return {
            'statusCode': 404,
            'body': json.dumps({'error': 'not found'})
        }
