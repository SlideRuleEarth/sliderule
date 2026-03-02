import os
import botocore.exceptions
import json
import base64
from datetime import datetime, timedelta, timezone
import manager

# ###############################
# Utilities
# ###############################

#
# Get tags for EC2 instances
#
def get_instances_by_name(name):
    try:
        response = manager.ec2.describe_instances(
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
    response = manager.ses.send_email(
        Source = support_email,
        Destination = {
            "ToAddresses": [alert_email]
        },
        Message = {
            "Subject": {
                "Data": f"SlideRule Cluster Deployed ({rqst['cluster']}/{rqst['node_capacity']}/{rqst['ttl']})"
            },
            "Body": { "Text": {
                "Data": message
            }}
        })
    if response["HTTPStatusCode"] >= 200 and response["HTTPStatusCode"] < 300:
        return True
    else:
        print(f"Error sending email from {support_email} to {alert_email}: {response}")
        return False

# ###############################
# Business Logic
# ###############################

#
# Max Nodes
#
def get_max_nodes(org_roles):
    """
    Returns the maximum number of nodes a user can deploy
    """
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
    """
    Returns the maximum time to live a user can deploy
    """
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
    max_nodes = int(claims.get('max_nodes', "0"))
    max_ttl = int(claims.get('max_ttl', "0"))
    audiences = parse_claim_array(claims.get('aud', "[]"))
    known_clusters = ['sliderule'] + [audience for audience in audiences if (audience != "*" and audience not in manager.SYSTEM_KEYWORDS)]
    deployable_clusters = [audience for audience in audiences if (audience not in manager.SYSTEM_KEYWORDS)]
    max_nodes = get_max_nodes(org_roles)
    max_ttl = get_max_ttl(org_roles)
    return {
        'userName': username,
        'orgRoles': org_roles,
        'knownClusters': ','.join(known_clusters),
        'deployableClusters': ','.join(deployable_clusters),
        'maxNodes': str(max_nodes),
        'maxTTL': str(max_ttl)
    }

#
# Validate Request
#
def validate_request(path, body, info):
        # check organization membership
        if 'member' not in info["orgRoles"]:
            print(f'Access denied to {info["userName"]}, organization roles: {info["orgRoles"]}')
            return None

        # check cluster
        cluster = body.get("cluster")
        if cluster and ((cluster not in info["deployableClusters"]) and ('*' not in info["deployableClusters"])):
            print(f'Access denied to {info["userName"]}, allowed clusters: {info["deployableClusters"]}')
            return None

        # check node_capacity
        node_capacity = body.get("node_capacity")
        if node_capacity and ((int(node_capacity) > info["maxNodes"]) or (int(node_capacity) < 1)):
            print(f'Access denied to {info["userName"]}, node capacity: {node_capacity}, max nodes: {info["maxNodes"]}')
            return None

        # check ttl
        ttl = body.get("ttl")
        if ttl and ((int(ttl) > info["maxTTL"]) or (int(ttl) < manager.MIN_TTL_FOR_AUTOSHUTDOWN)):
            print(f'Access denied to {info["userName"]}, invalid ttl: {ttl}')
            return None

        # build and return request info
        return {
            "owner": 'owner' in info["orgRoles"],
            "cluster": cluster,
            "node_capacity": node_capacity,
            "ttl": ttl,
            "is_public": body.get("is_public", False),
            "version": body.get("version", "latest"),
            'branch': body.get("branch", "main")
        }

# ###############################
# Path Handlers
# ###############################

#
# Deploy
#
def deploy_handler(rqst):

    try:
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
        resp = manager.cf.describe_stacks(StackName=stack_name)
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
        stack_name = manager.build_stack_name(rqst["cluster"])

        # create stack
        state["response"] = manager.cf.create_stack(StackName=stack_name, TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

        # status deployment
        send_email(f"SlideRule Cluster Deployed ({rqst['cluster']}/{rqst['node_capacity']}/{rqst['ttl']})", json.dumps(state, indent=2))
        print(f'Deploy initiated for {stack_name}')

        # return success
        return json_response(200, state)

    except Exception as e:

        # failure
        return exception_reponse(e)

#
# Extend
#
def extend_handler(rqst):

    try:

        # initialize response state
        state = {}

        # get rule name
        rule_name = f'{manager.build_stack_name(rqst["cluster"])}-auto-shutdown'

        # calculate new shutdown time
        new_shutdown_time = datetime.now(timezone.utc) + timedelta(minutes=rqst["ttl"])
        state["cron_expression"] = f'cron({new_shutdown_time.minute} {new_shutdown_time.hour} {new_shutdown_time.day} {new_shutdown_time.month} ? {new_shutdown_time.year})'

        # extend rule
        state["response"] = manager.ev.put_rule(
            Name=rule_name,
            ScheduleExpression=state["cron_expression"],
            State='ENABLED',
            Description=f'Automatic shutdown for {rule_name} (updated)'
        )

        # return success
        return json_response(200, state)

    except Exception as e:

        # failure
        return exception_reponse(e)


#
# Status
#
def status_handler(rqst):

    try:
        # initialize response state
        state = {}

        # status stack
        stack_name = manager.build_stack_name(rqst["cluster"])
        description = manager.cf.describe_stacks(StackName=stack_name)
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
        rule_name = manager.build_rule_name(stack_name)
        state["auto_shutdown"] = manager.get_next_trigger_time(rule_name)

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

    except Exception as e:

        # failure
        return exception_reponse(e)

#
# Events
#
def events_handler(rqst):

    try:
        # get events for stack
        stack_name = manager.build_stack_name(rqst["cluster"])
        description = manager.cf.describe_stack_events(StackName=stack_name)
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

    except Exception as e:

        # failure
        return exception_reponse(e)


#
# Cluster Report
#
def report_clusters_handler(rqst):

    try:
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
                details = status_handler({"cluster": cluster, "region": region}, None)
                if details["status"]:
                    report[cluster] = { k: details.get(k) for k in ["auto_shutdown", "current_nodes", "version", "is_public", "node_capacity"] }

        # return success
        return json_response(200, report)

    except Exception as e:

        # failure
        return exception_reponse(e)

#
# Test Report
#
def report_tests_handler(rqst):

    try:
        # get environment variables
        project_bucket = os.environ["PROJECT_BUCKET"]
        project_folder = os.environ["PROJECT_FOLDER"]

        # get test summary
        summary_file = f"{rqst["branch"]}-summary.json"
        manager.s3.download_file(Bucket=project_bucket, Key=f"{project_folder}/testrunner/{summary_file}", Filename=f"/tmp/{summary_file}")

        # read test summary
        with open(f"/tmp/{summary_file}", "r") as file:
            report = json.loads(file.read())

        # return success
        return json_response(200, report)

    except Exception as e:

        # failure
        return exception_reponse(e)

#
# Test Runner
#
def test_handler(rqst):

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
        resp = manager.cf.describe_stacks(StackName=stack_name)
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
        state["response"] = manager.cf.create_stack(StackName='testrunner', TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

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
        path = event.get('rawPath', '') # get path of request
        body = get_body(event) # get body of request
        info = get_user_info(claims) # pull out user information from claims
        rqst = validate_request(path, body, info) # validate request against claims and return safe request parameters
        print(f'Received request: {path} {body}') # diagnostic

        # route request
        if rqst == None: # check request
            return json_response(403, {'error': 'access denied'})
        elif path == '/info': # only uses info, but still validate rqst
            return json_response(200, info)
        elif path == '/deploy':
            return deploy_handler(rqst)
        elif path == '/extend':
            return extend_handler(rqst)
        elif path == '/destroy':
            return manager.lambda_destroy(rqst)
        elif path == '/status':
            return status_handler(rqst)
        elif path == '/events':
            return events_handler(rqst)
        elif rqst["owner"]: # owner level APIs
            if path == '/report/clusters' or path == '/report':
                return report_clusters_handler(rqst)
            elif path == '/report/tests':
                return report_tests_handler(rqst)
            elif path == '/test':
                return test_handler(rqst)

        # invalid path
        return json_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return exception_reponse(e)
