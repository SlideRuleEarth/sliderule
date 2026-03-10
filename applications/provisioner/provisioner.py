import os
import re
import json
import base64
import boto3
import urllib3
import botocore.exceptions
from datetime import datetime, timedelta, timezone
from cryptography.hazmat.primitives.serialization import load_ssh_public_key

# ###############################
# Cached Objects
# ###############################

ec2 = boto3.client("ec2")
s3 = boto3.client("s3")
cf = boto3.client("cloudformation")
ev = boto3.client('events')
la = boto3.client('lambda')
ses = boto3.client('ses')
sm = boto3.client('secretsmanager')

_http = urllib3.PoolManager()

_pubkey_cache = {}

# ###############################
# Globals
# ###############################

SYSTEM_KEYWORDS = ['login','provisioner','client','recorder','runner','mcp']
SUCCESS = "SUCCESS"
FAILED = "FAILED"

# ###############################
# Utilities
# ###############################

#
# Convention for deriving rule name from stack name
#
def build_rule_name(stack_name):
    return f'{stack_name}-auto-shutdown'

#
# Checks on cluster name
#
def valid_cluster_name(cluster):
    if (cluster in SYSTEM_KEYWORDS) or \
       (not re.match(r'^[A-Za-z0-9-_]+$', cluster)) or \
       (len(cluster) > 40):
        return False
    else:
        return True

#
# Convention for deriving stack name from cluster
#
def build_stack_name(cluster):
    if not isinstance(cluster, str):
        return None
    elif valid_cluster_name(cluster):
        return f'{cluster}-cluster'
    else:
        raise RuntimeError(f"Invalid cluster name: {cluster}")

#
# Get tags for EC2 instances
#
def get_instances_by_name(name):
    try:
        response = ec2.describe_instances(
            Filters=[
                {"Name": "tag:Name", "Values": [name]},
                {"Name": "instance-state-name", "Values": ["pending", "running", "stopped"]}
            ]
        )
        return [instance for reservation in response["Reservations"] for instance in reservation["Instances"]]
    except Exception as e:
        print(f"Unable to get <{name}> instances: {e}")
        return []

#
# Custom parsing code for API Gateway arrays
#
def parse_claim_array(claim_value):
    if isinstance(claim_value, str):
        if claim_value.startswith('['):
            return claim_value.strip('[]').split() # Remove brackets and split by whitespace
        else:
            return [claim_value]
    else:
        return []

#
# Handle encoded body
#
def get_body(event):
    body_raw = event.get("body")
    if body_raw:
        if event.get("isBase64Encoded"):
            body_raw = base64.b64decode(body_raw).decode("utf-8")
        return json.loads(body_raw)
    else:
        return {}

#
# API Gateway Response Format
#
def json_response(status_code, body):
    return {
        'statusCode': status_code,
        'headers': {
            'Content-Type': 'application/json',
        },
        'body': json.dumps(body)
    }

#
# Boto3 Error Handling
#
def exception_reponse(e):
    print(f'Exception: {e}')
    if isinstance(e, botocore.exceptions.ClientError) and (e.response['Error']['Code'] == 'ValidationError'):
        return json_response(404, {'error': 'not found', 'error_description': 'failure in request'})
    return json_response(500, {'error': 'internal error', 'error_description': 'failure in request'})

#
# Send Email Message
#
def send_email(title, message):
    support_email = os.environ['SUPPORT_EMAIL']
    alert_email = os.environ['ALERT_EMAIL']
    response = ses.send_email(
        Source = support_email,
        Destination = {
            "ToAddresses": [alert_email]
        },
        Message = {
            "Subject": {
                "Data": title
            },
            "Body": { "Text": {
                "Data": message
            }}
        })
    if response["ResponseMetadata"]["HTTPStatusCode"] >= 200 and response["ResponseMetadata"]["HTTPStatusCode"] < 300:
        return True
    else:
        print(f"Error sending email from {support_email} to {alert_email}: {response}")
        return False

# ###############################
# Business Logic
# ###############################

def verify_signature(path, body, username, event):
    """
    Verifies request signature using public key
    """
    global _pubkey_cache
    try:
        # pull out signing parameters from request
        host = event["headers"]["host"]
        timestamp = event["headers"]["x-sliderule-timestamp"]
        signature_b64 = event["headers"]["x-sliderule-signature"]
        signature = base64.b64decode(signature_b64)

        # get public key
        if username not in _pubkey_cache:
            domain = os.environ["DOMAIN"]
            secret_name = f"{domain}/pubkeys"
            secret_string = sm.get_secret_value(SecretId=secret_name)['SecretString']
            _pubkey_cache = json.loads(secret_string)
        public_key = load_ssh_public_key(_pubkey_cache[username].encode("utf-8"))

        # build canonical message
        full_path = f'{host}{path}'
        full_path_b64 = base64.urlsafe_b64encode(full_path.encode()).decode()
        body_b64 = base64.urlsafe_b64encode(body.encode()).decode()
        canonical_string = f"{full_path_b64}:{timestamp}:{body_b64}"
        message_bytes = canonical_string.encode("utf-8")

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

#
# Max Nodes
#
def get_max_nodes(org_roles):
    max_nodes = 0
    if 'owner' in org_roles:
        max_nodes = 100
    elif 'member' in org_roles:
        max_nodes = 50
    return max_nodes

#
# Max TTL
#
def get_max_ttl(org_roles):
    max_ttl = 0
    if 'owner' in org_roles:
        max_ttl = 525600 # 1 year
    elif 'member' in org_roles:
        max_ttl = 720 # 12 hours
    return max_ttl

#
# Get User Info
#
def get_user_info(claims):
    username = claims.get('sub', '<anonymous>')
    org_roles = parse_claim_array(claims.get('org_roles', "[]"))
    audiences = parse_claim_array(claims.get('aud', "[]"))
    deployable_clusters = {audience for audience in audiences if valid_cluster_name(audience)}
    known_clusters = deployable_clusters - {'*'} | {'sliderule'}
    max_nodes = get_max_nodes(org_roles)
    max_ttl = get_max_ttl(org_roles)
    return {
        'username': username,
        'orgRoles': org_roles,
        'knownClusters': ','.join(known_clusters),
        'deployableClusters': ','.join(deployable_clusters),
        'maxNodes': max_nodes,
        'maxTTL': max_ttl
    }

#
# Validate Request
#
def validate_request(event, info):

    # configuration
    MIN_TTL_FOR_AUTOSHUTDOWN = 15 # minutes

    # pull out required request parameters
    path = event.get("rawPath", '')
    body_raw = event.get("body", '')
    if event.get("isBase64Encoded"):
        body_raw = base64.b64decode(body_raw).decode("utf-8")
    if body_raw:
        body = json.loads(body_raw)
    else:
        body = {}
    print(f'Received request: {path} {body}') # diagnostic

    # check signature (for owners only)
    if ("owner" in info["orgRoles"]) or ('*' in info["deployableClusters"]):
        if not verify_signature(path, body_raw, info["username"], event):
            return None

    # check organization membership
    if 'member' not in info["orgRoles"]:
        print(f'Access denied to {info["username"]}, organization roles: {info["orgRoles"]}')
        return None

    # check cluster
    cluster = body.get("cluster")
    if cluster and ((cluster not in info["deployableClusters"]) and ('*' not in info["deployableClusters"])):
        print(f'Access denied to {info["username"]}, allowed clusters: {info["deployableClusters"]}')
        return None

    # check node_capacity
    node_capacity = body.get("node_capacity")
    if node_capacity and ((int(node_capacity) > info["maxNodes"]) or (int(node_capacity) < 1)):
        print(f'Access denied to {info["username"]}, node capacity: {node_capacity}, max nodes: {info["maxNodes"]}')
        return None

    # check ttl
    ttl = body.get("ttl")
    if ttl and ((int(ttl) > info["maxTTL"]) or (int(ttl) < MIN_TTL_FOR_AUTOSHUTDOWN)):
        print(f'Access denied to {info["username"]}, invalid ttl: {ttl}')
        return None

    # build and return request info
    return {
        "path": path,
        "username": info["username"],
        "owner": 'owner' in info["orgRoles"],
        "cluster": cluster,
        "node_capacity": node_capacity,
        "ttl": ttl,
        "is_public": body.get("is_public", False),
        "version": body.get("version", "latest"),
        'branch': body.get("branch", "main")
    }

# ###############################
# Lambda: Schedule Auto Shutdown
# ###############################

#
# Cloud Formation Response
#
def cfn_send(event, context, responseStatus, responseData, physicalResourceId=None, noEcho=False, reason=None):
    responseUrl = event['ResponseURL']

    responseBody = {
        'Status' : responseStatus,
        'Reason' : reason or "See the details in CloudWatch Log Stream: {}".format(context.log_stream_name),
        'PhysicalResourceId' : physicalResourceId or context.log_stream_name,
        'StackId' : event['StackId'],
        'RequestId' : event['RequestId'],
        'LogicalResourceId' : event['LogicalResourceId'],
        'NoEcho' : noEcho,
        'Data' : responseData
    }

    json_responseBody = json.dumps(responseBody)

    headers = {
        'content-type' : '',
        'content-length' : str(len(json_responseBody))
    }

    try:
        response = _http.request('PUT', responseUrl, headers=headers, body=json_responseBody)
        print("Status code:", response.status)
    except Exception as e:
        print("send(..) failed executing http.request(..):", e)

#
# Get the next time of the event bridge rule
#
def get_next_trigger_time(rule_name):
    """
    Return next trigger for cron() expressions in an EventBridge rule.
    Example: cron(0 12 * * ? *) - daily at noon UTC
    """
    try:
        from croniter import croniter

        # Get the rule details
        response = ev.describe_rule(Name=rule_name)

        # Check if rule is enabled
        state = response.get('State')
        if state != 'ENABLED':
            print(f"Rule {rule_name} is {state}")
            return None

        # Get the schedule expression
        schedule_expression = response.get('ScheduleExpression')
        if not schedule_expression:
            print(f"Rule {rule_name} is not a scheduled rule (it's event-based)")
            return None

        # Check schedule expression format
        if not schedule_expression.startswith('cron('):
            print(f"Unknown schedule expression format: {schedule_expression}")
            return None

        # Calculate next trigger
        # EventBridge format: cron(Minutes Hours Day-of-month Month Day-of-week Year)
        cron_expr = schedule_expression.replace('cron(', '').replace(')', '')

        # Convert EventBridge cron to standard cron format
        # EventBridge: Minutes Hours Day Month DayOfWeek Year
        # Standard:    Minutes Hours Day Month DayOfWeek
        parts = cron_expr.split()
        if len(parts) != 6:
            print(f"Invalid cron expression format: {cron_expr}")
            return None

        # Remove the Year field and convert ? to *
        standard_cron = ' '.join(parts[:5]).replace('?', '*')

        # Calculate next occurrence
        now = datetime.now(timezone.utc)
        cron = croniter(standard_cron, now)

        # Return next time
        return cron.get_next(datetime).isoformat()

    except ImportError:
        print("croniter library not installed. Install with: pip install croniter")
        return None
    except ev.exceptions.ResourceNotFoundException:
        print(f"Rule {rule_name} not found")
        return None
    except Exception as e:
        print(f"Error getting rule details: {e}")
        return None

#
# Schedule
#
def lambda_schedule(event, context):
    print(f'Schedule Event: {json.dumps(event)}')

    try:
        # don't schedule deletion if stack is already being deleted
        if event['RequestType'] == 'Delete':
            print('Stack is being deleted, skipping scheduler')
            cfn_send(event, context, SUCCESS, {})
            return

        # handle creation and update events
        if event['RequestType'] in ['Create', 'Update']:
            stack_name = event['ResourceProperties']['StackName']
            delete_after_minutes = int(event['ResourceProperties']['DeleteAfterMinutes'])
            deletion_lambda_arn = event['ResourceProperties']['DeletionLambdaArn']
            rule_name = build_rule_name(stack_name)

            # calculate schedule time
            shutdown_time = datetime.utcnow() + timedelta(minutes=delete_after_minutes)
            cron_expression = f'cron({shutdown_time.minute} {shutdown_time.hour} {shutdown_time.day} {shutdown_time.month} ? {shutdown_time.year})'

            # create eventbridge rule
            ev.put_rule(
                Name=rule_name,
                ScheduleExpression=cron_expression,
                State='ENABLED',
                Description=f'Automatically shutdown stack {stack_name}'
            )

            # add lambda permission
            try:
                la.add_permission(
                    FunctionName=deletion_lambda_arn,
                    StatementId=f'{rule_name}-permission',
                    Action='lambda:InvokeFunction',
                    Principal='events.amazonaws.com',
                    SourceArn=f'arn:aws:events:{context.invoked_function_arn.split(":")[3]}:{context.invoked_function_arn.split(":")[4]}:rule/{rule_name}'
                )
            except la.exceptions.ResourceConflictException:
                print('Permission already exists')

            # add target
            ev.put_targets(
                Rule=rule_name,
                Targets=[{
                    'Id': '1',
                    'Arn': deletion_lambda_arn,
                    'Input': json.dumps({'stack_name': stack_name})
                }]
            )

            # acknowledge hook complete back to cloudformation
            print(f'Scheduled stack deletion at {shutdown_time} UTC')
            response_data = {
                'ScheduledTime': shutdown_time.isoformat(),
                'RuleName': rule_name
            }
            cfn_send(event, context, SUCCESS, response_data)

    except Exception as e:

        # acknowledge hook error back to cloudformation
        print(f'Error in custom scheduler: {e}')
        cfn_send(event, context, FAILED, {'Error': str(e)})

# ###############################
# Path Handlers
# ###############################

#
# Deploy
#
def deploy_handler(rqst):

    # initialize response state
    state = {}

    # get environment variables
    stack_name = os.environ["STACK_NAME"]
    environment_version = os.environ['ENVIRONMENT_VERSION']
    domain = os.environ["DOMAIN"]
    project_bucket = os.environ["PROJECT_BUCKET"]
    project_folder = os.environ["PROJECT_FOLDER"]
    project_public_bucket = os.environ["PROJECT_PUBLIC_BUCKET"]
    container_registry = os.environ['CONTAINER_REGISTRY']
    jwt_issuer = os.environ['JWT_ISSUER']
    alert_stream = os.environ['ALERT_STREAM']
    telemetry_stream = os.environ['TELEMETRY_STREAM']

    # get arns for auto-shutdown
    resp = cf.describe_stacks(StackName=stack_name)
    outputs = resp["Stacks"][0].get("Outputs", [])
    destroy_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "DestroyLambdaArn")
    schedule_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "ScheduleLambdaArn")

    # build parameters for stack creation
    state["parms"] = [
        {"ParameterKey": "Version", "ParameterValue": rqst["version"]},
        {"ParameterKey": "IsPublic", "ParameterValue": json.dumps(rqst["is_public"])},
        {"ParameterKey": "Cluster", "ParameterValue": rqst["cluster"]},
        {"ParameterKey": "NodeCapacity", "ParameterValue": str(rqst["node_capacity"])},
        {"ParameterKey": "TTL", "ParameterValue": str(rqst["ttl"])},
        {"ParameterKey": "EnvironmentVersion", "ParameterValue": environment_version},
        {"ParameterKey": "Domain", "ParameterValue": domain},
        {"ParameterKey": "ProjectBucket", "ParameterValue": project_bucket},
        {"ParameterKey": "ProjectFolder", "ParameterValue": project_folder},
        {"ParameterKey": "ProjectPublicBucket", "ParameterValue": project_public_bucket},
        {"ParameterKey": "DestroyLambdaArn", "ParameterValue": destroy_lambda_arn},
        {"ParameterKey": "ScheduleLambdaArn", "ParameterValue": schedule_lambda_arn},
        {"ParameterKey": "ContainerRegistry", "ParameterValue": container_registry},
        {"ParameterKey": "JwtIssuer", "ParameterValue": jwt_issuer},
        {"ParameterKey": "AlertStream", "ParameterValue": alert_stream},
        {"ParameterKey": "TelemetryStream", "ParameterValue": telemetry_stream},
    ]

    # read template
    templateBody = open("cluster.yml").read()

    # the stack name naming convention is required by Makefile
    stack_name = build_stack_name(rqst["cluster"])

    # create stack
    state["response"] = cf.create_stack(StackName=stack_name, TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

    # status deployment
    send_email(f"SlideRule Cluster Deployed ({rqst['cluster']}/{rqst['node_capacity']}/{rqst['ttl']})", json.dumps(state, indent=2))
    print(f'Deploy initiated for {stack_name}')

    # return successa
    return json_response(200, state)


#
# Extend
#
def extend_handler(rqst):

    # initialize response state
    state = {}

    # get rule name
    rule_name = f'{build_stack_name(rqst["cluster"])}-auto-shutdown'

    # calculate new shutdown time
    new_shutdown_time = datetime.now(timezone.utc) + timedelta(minutes=rqst["ttl"])
    state["cron_expression"] = f'cron({new_shutdown_time.minute} {new_shutdown_time.hour} {new_shutdown_time.day} {new_shutdown_time.month} ? {new_shutdown_time.year})'

    # extend rule
    state["response"] = ev.put_rule(
        Name=rule_name,
        ScheduleExpression=state["cron_expression"],
        State='ENABLED',
        Description=f'Automatic shutdown for {rule_name} (updated)'
    )

    # return success
    return json_response(200, state)

#
# Destroy
#
def lambda_destroy(event, context):

    try:
        # initialize response status
        state = {"status": True}

        # get optional request variables
        cluster = event.get("cluster") # schedule deletions do not supply cluster parameter
        state["stack_name"] = event.get("stack_name", build_stack_name(cluster)) # scheduled deletions pass stack name

        # delete eventbridge target and rule
        rule_name = f'{state["stack_name"]}-auto-shutdown'
        print(f'Delete initiated for {rule_name}')
        try:
            ev.remove_targets(Rule=rule_name, Ids=["1"])
            state["EventBridge Target Removed"] = True
        except Exception as e:
            print(f'Unable to delete eventbridge rule: {e}')
            state["EventBridge Target Removed"] = False
        try:
            ev.delete_rule(Name=rule_name, Force=False)
            state["EventBridge Rule Removed"] = True
        except Exception as e:
            print(f'Unable to delete eventbridge rule: {e}')
            state["EventBridge Rule Removed"] = False

        # delete stack
        state["response"] = cf.delete_stack(StackName=state["stack_name"])
        print(f'Delete initiated for {state["stack_name"]}')

    # explicipty handle exceptions
    except Exception as e:
        print(f'Exception in destroy: {e}')
        state["exception"] = f'Failure in destroy'
        state["status"] = False

    # return response directly
    return state


#
# Status
#
def status_handler(rqst):

    # initialize response state
    state = {}

    # status stack
    stack_name = build_stack_name(rqst["cluster"])
    description = cf.describe_stacks(StackName=stack_name)
    stack = description["Stacks"][0]

    # build cleaned response
    response = {}
    for k, v in stack.items():
        if hasattr(v, "tolist"):
            response[k] = v.tolist()
        elif isinstance(v, (datetime)):
            response[k] = v.isoformat()
        elif not isinstance(v, (bytes, bytearray)):
            response[k] = v
    state["response"] = response

    # attempt to get auto-shutdown time
    rule_name = build_rule_name(stack_name)
    state["auto_shutdown"] = get_next_trigger_time(rule_name)

    # get number of nodes
    node_instances = get_instances_by_name(f'{rqst["cluster"]}-node')
    state["current_nodes"] = len(node_instances)

    # get version and node capacity
    ilb_instance = get_instances_by_name(f'{rqst["cluster"]}-ilb')
    if len(ilb_instance) == 1:
        tags = ilb_instance[0]["Tags"]
        for tag in tags:
            if tag["Key"] == "Version":
                state["version"] = tag["Value"]
            if tag["Key"] == "IsPublic":
                state["is_public"] = tag["Value"]
            if tag["Key"] == "NodeCapacity":
                state["node_capacity"] = tag["Value"]

    # return success
    return json_response(200, state)


#
# Events
#
def events_handler(rqst):

    # get events for stack
    stack_name = build_stack_name(rqst["cluster"])
    description = cf.describe_stack_events(StackName=stack_name)
    stack_events = description["StackEvents"]
    print(f'Events requested for {stack_name}')

    # build cleaned response
    response = []
    for e in stack_events:
        stack_event = {}
        for k, v in e.items():
            if hasattr(v, "tolist"):
                stack_event[k] = v.tolist()
            elif isinstance(v, (datetime)):
                stack_event[k] = v.isoformat()
            elif not isinstance(v, (bytes, bytearray)):
                stack_event[k] = v
        response.append(stack_event)

    # return success
    return json_response(200, response)


#
# Cluster Report
#
def report_clusters_handler(rqst):

    # initialize report
    report = {}

    # get environment variables
    region = os.environ["AWS_REGION"]

    # get list of intelligent load balancers with their tags
    for instance in get_instances_by_name('*-ilb'):
        tags = {t['Key']: t['Value'] for t in instance.get('Tags', [])}
        name = tags.get('Name')
        if name:
            # get status of each cluster containing the intelligent load balancer
            cluster = name.split("-ilb")[0]
            details = status_handler({"cluster": cluster, "region": region})
            if details["statusCode"] == 200:
                status = json.loads(details["body"])
                report[cluster] = { k: status.get(k) for k in ["auto_shutdown", "current_nodes", "version", "is_public", "node_capacity"] }

    # return success
    return json_response(200, report)


#
# Test Report
#
def report_tests_handler(rqst):

    # get environment variables
    project_bucket = os.environ["PROJECT_BUCKET"]
    project_folder = os.environ["PROJECT_FOLDER"]

    # get test summary
    summary_file = f"{rqst["branch"]}-summary.json"
    s3.download_file(Bucket=project_bucket, Key=f"{project_folder}/testrunner/{summary_file}", Filename=f"/tmp/{summary_file}")

    # read test summary
    with open(f"/tmp/{summary_file}", "r") as file:
        report = json.loads(file.read())

    # return success
    return json_response(200, report)


#
# Test Runner
#
def deploy_test_handler(rqst):

    try:
        # initialize response status
        state = {}

        # get environment variables
        stack_name = os.environ["STACK_NAME"]
        domain = os.environ["DOMAIN"]
        environment_version = os.environ['ENVIRONMENT_VERSION']
        project_bucket = os.environ["PROJECT_BUCKET"]
        project_folder = os.environ["PROJECT_FOLDER"]
        container_registry = os.environ['CONTAINER_REGISTRY']

        # get arns for auto-shutdown
        resp = cf.describe_stacks(StackName=stack_name)
        outputs = resp["Stacks"][0].get("Outputs", [])
        destroy_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "DestroyLambdaArn")
        scheduler_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "ScheduleLambdaArn")

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Domain", "ParameterValue": domain},
            {"ParameterKey": "EnvironmentVersion", "ParameterValue": environment_version},
            {"ParameterKey": "ProjectBucket", "ParameterValue": project_bucket},
            {"ParameterKey": "ProjectFolder", "ParameterValue": project_folder},
            {"ParameterKey": "ContainerRegistry", "ParameterValue": container_registry},
            {"ParameterKey": "DestroyLambdaArn", "ParameterValue": destroy_lambda_arn},
            {"ParameterKey": "ScheduleLambdaArn", "ParameterValue": scheduler_lambda_arn},
            {"ParameterKey": "DeployDate", "ParameterValue": datetime.now().strftime("%Y%m%d%H%M%S")},
            {"ParameterKey": "Branch", "ParameterValue": rqst["branch"]}
        ]

        # read template
        templateBody = open("testrunner.yml").read()

        # create stack (default to hardcoded stack name so only one can run at a time)
        state["response"] = cf.create_stack(StackName='testrunner', TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

        # success
        return json_response(200, state)

    except Exception as e:

        # failure
        return exception_reponse(e)

# ###############################
# Lambda: Gateway Handler
# ###############################

def lambda_gateway(event, context):
    """
    Route requests based on path
    """
    try:
        # process request
        claims = event["requestContext"]["authorizer"]["jwt"]["claims"] # get JWT claims (validated by API Gateway)
        info = get_user_info(claims) # pull out user information from claims
        rqst = validate_request(event, info) # validate request against claims and return safe request parameters

        # route request
        if rqst == None: # check request
            return json_response(403, {'error': 'access denied'})
        elif rqst["path"] == '/info': # only uses info, but still requires validated rqst
            return json_response(200, info)
        elif rqst["path"] == '/deploy': # deploys a cluster
            return deploy_handler(rqst)
        elif rqst["path"] == '/extend': # extends the TTL of a cluster
            return extend_handler(rqst)
        elif rqst["path"] == '/destroy': # destroys a cluster
            return lambda_destroy(rqst, None)
        elif rqst["path"] == '/status': # returns deployment status of a cluster
            return status_handler(rqst)
        elif rqst["path"] == '/events': # returns cloudformation stack events for a cluster deployment
            return events_handler(rqst)
        elif rqst["path"] == '/report/clusters' or rqst["path"] == '/report': # returns report of clusters that have been deployed
            return report_clusters_handler(rqst)
        elif rqst["path"] == '/report/tests': # returns report on status of last test runner deployment
            return report_tests_handler(rqst)
        elif rqst["path"] == '/test': # deploys test runner
            return deploy_test_handler(rqst)

        # invalid path
        return json_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return exception_reponse(e)
