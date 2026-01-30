import os
import boto3
import botocore.exceptions
import json
import base64
from datetime import datetime, timedelta
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

# ###############################
# Path Handlers
# ###############################

#
# Deploy
#
def deploy_handler(event, context):

    # initialize response state
    state = {"status": True}

    try:
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

        # get required request variables
        cluster = event["cluster"]
        is_public = event["is_public"]
        node_capacity = event["node_capacity"]
        ttl = event["ttl"]

        # get optional request variables
        version = event.get("version", "latest")

        # get arns for auto-shutdown
        resp = manager.cf.describe_stacks(StackName=stack_name)
        outputs = resp["Stacks"][0].get("Outputs", [])
        destroy_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "DestroyLambdaArn")
        schedule_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "ScheduleLambdaArn")

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Version", "ParameterValue": version},
            {"ParameterKey": "IsPublic", "ParameterValue": json.dumps(is_public)},
            {"ParameterKey": "Cluster", "ParameterValue": cluster},
            {"ParameterKey": "NodeCapacity", "ParameterValue": str(node_capacity)},
            {"ParameterKey": "TTL", "ParameterValue": str(ttl)},
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
        state["stack_name"] = manager.build_stack_name(cluster)

        # create stack
        state["response"] = manager.cf.create_stack(StackName=state["stack_name"], TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])
        print(f'Deploy initiated for {state["stack_name"]}')

    except RuntimeError as e:
        print(f'User error in deploy: {e}')
        state["exception"] = f'User error in deploy'
        state["status"] = False

    except Exception as e:
        print(f'Exception in deploy: {e}')
        state["exception"] = f'Failure in deploy'
        state["status"] = False

    # return response
    return state

#
# Extend
#
def extend_handler(event, context):

    # initialize response state
    state = {"status": True}

    try:

        # get required request variables
        cluster = event["cluster"]
        ttl = event["ttl"]

        # get rule name
        rule_name = f'{manager.build_stack_name(cluster)}-auto-shutdown'

        # calculate new shutdown time
        new_shutdown_time = datetime.utcnow() + timedelta(minutes=ttl)
        state["cron_expression"] = f'cron({new_shutdown_time.minute} {new_shutdown_time.hour} {new_shutdown_time.day} {new_shutdown_time.month} ? {new_shutdown_time.year})'

        # extend rule
        state["response"] = manager.ev.put_rule(
            Name=rule_name,
            ScheduleExpression=state["cron_expression"],
            State='ENABLED',
            Description=f'Automatic shutdown for {rule_name} (updated)'
        )

    except botocore.exceptions.ClientError as e:
        print(f'Client error: {e}')
        if e.response['Error']['Code'] == 'ValidationError':
            state["exception"] = f'Not found'
        else:
            state["exception"] = f'Unexpected error'
        state["status"] = False

    except Exception as e:
        print(f'Exception in extend: {e}')
        state["exception"] = f'Failure in extend'
        state["status"] = False

    # return response
    return state

#
# Status
#
def status_handler(event, context):

    # initialize response state
    state = {"status": True}

    try:
        # get required request variables
        cluster = event["cluster"]

        # get stack name
        state["stack_name"] = manager.build_stack_name(cluster)

        # status stack
        print(f'Status requested for {state["stack_name"]}')
        description = manager.cf.describe_stacks(StackName=state["stack_name"])
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
        rule_name = manager.build_rule_name(state["stack_name"])
        state["auto_shutdown"] = manager.get_next_trigger_time(rule_name)

        # get number of nodes
        node_instances = get_instances_by_name(f'{cluster}-node')
        state["current_nodes"] = len(node_instances)

        # get version and node capacity
        ilb_instance = get_instances_by_name(f'{cluster}-ilb')
        if len(ilb_instance) == 1:
            tags = ilb_instance[0]["Tags"]
            for tag in tags:
                if tag["Key"] == "Version":
                    state["version"] = tag["Value"]
                if tag["Key"] == "IsPublic":
                    state["is_public"] = tag["Value"]
                if tag["Key"] == "NodeCapacity":
                    state["node_capacity"] = tag["Value"]

    except botocore.exceptions.ClientError as e:
        print(f'Client error: {e}')
        if e.response['Error']['Code'] == 'ValidationError':
            state["exception"] = f'Not found'
        else:
            state["exception"] = f'Unexpected error'
        state["status"] = False

    except Exception as e:
        print(f'Exception in status: {e}')
        state["exception"] = f'Failure in status'
        state["status"] = False

    # return response
    return state

#
# Events
#
def events_handler(event, context):

    # initialize response state
    state = {"status": True}

    try:
        # get required request variables
        cluster = event["cluster"]

        # get stack name
        state["stack_name"] = manager.build_stack_name(cluster)

        # get events for stack
        description = manager.cf.describe_stack_events(StackName=state["stack_name"])
        stack_events = description["StackEvents"]
        print(f'Events requested for {state["stack_name"]}')

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
        state["response"] = response

    except botocore.exceptions.ClientError as e:
        print(f'Client error: {e}')
        if e.response['Error']['Code'] == 'ValidationError':
            state["exception"] = f'Not found'
        else:
            state["exception"] = f'Unexpected error'
        state["status"] = False

    except Exception as e:
        print(f'Exception in events: {e}')
        state["exception"] = f'Failure in events'
        state["status"] = False

    # return response
    return state

#
# Cluster Report
#
def report_clusters_handler(event, context):

    # initialize response state
    state = {"status": True, "report": {}}

    try:
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
                    state["report"][cluster] = { k: details.get(k) for k in ["auto_shutdown", "current_nodes", "version", "is_public", "node_capacity"] }

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f'Exception in report: {e}')
        state["exception"] = f'Failure in report'
        state["status"] = False

    # return response
    return state

#
# Test Report
#
def report_tests_handler(event, context):

    # initialize response state
    state = {"status": True, "report": {}}

    try:
        # get environment variables
        project_bucket = os.environ["PROJECT_BUCKET"]
        project_folder = os.environ["PROJECT_FOLDER"]

        # get optional request variables
        branch = event.get("branch", "main")

        # get test summary
        summary_file = f"{branch}-summary.json"
        manager.s3.download_file(Bucket=project_bucket, Key=f"{project_folder}/testrunner/{summary_file}", Filename=f"/tmp/{summary_file}")

        # read test summary
        with open(f"/tmp/{summary_file}", "r") as file:
            state["report"] = file.read()

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f'Exception in report: {e}')
        state["exception"] = f'Failure in report'
        state["status"] = False

    # return response
    return state

#
# Test Runner
#
def test_handler(event, context):

    # initialize response status
    state = {"status": True}

    try:
        # get environment variables
        stack_name = os.environ["STACK_NAME"]
        domain = os.environ["DOMAIN"]
        environment_version = os.environ['ENVIRONMENT_VERSION']
        project_bucket = os.environ["PROJECT_BUCKET"]
        project_folder = os.environ["PROJECT_FOLDER"]
        container_registry = os.environ['CONTAINER_REGISTRY']

        # get optional request variables
        deploy_date = event.get("deploy_date", datetime.now().strftime("%Y%m%d%H%M%S"))
        branch = event.get("branch", "main")
        state["stack_name"] = event.get('stack_name', 'testrunner') # default to hardcoded stack name so only one can run at a time

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
            {"ParameterKey": "DeployDate", "ParameterValue": deploy_date},
            {"ParameterKey": "Branch", "ParameterValue": branch}
        ]

        # read template
        templateBody = open("testrunner.yml").read()

        # create stack
        state["response"] = manager.cf.create_stack(StackName=state["stack_name"], TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f'Exception in test: {e}')
        state["exception"] = f'Failure in test runner'
        state["status"] = False

    # return response
    return state

# ###############################
# Lambda: Gateway Handler
# ###############################

def lambda_gateway(event, context):
    """
    Route requests based on path
    """
    # get JWT claims (validated by API Gateway)
    try:
        claims = event["requestContext"]["authorizer"]["jwt"]["claims"]
        username = claims.get('sub', '<anonymous>')
        org_roles = parse_claim_array(claims.get('org_roles', "[]"))
        max_nodes = int(claims.get('max_nodes', "0"))
        max_ttl = int(claims.get('max_ttl', "0"))
        audiences = parse_claim_array(claims.get('aud', "[]"))
        allowed_clusters = [audience for audience in audiences if audience not in manager.SYSTEM_KEYWORDS]

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

        # check organization membership
        if 'member' not in org_roles:
            print(f'Access denied to {username}, organization roles: {org_roles}')
            return {
                'statusCode': 403,
                'body': json.dumps({'error': 'access denied'})
            }

        # check cluster
        cluster = body.get("cluster")
        if cluster and ((cluster not in allowed_clusters) and ('*' not in allowed_clusters)):
            print(f'Access denied to {username}, allowed clusters: {allowed_clusters}')
            return {
                'statusCode': 403,
                'body': json.dumps({'error': 'access denied'})
            }

        # check node_capacity
        node_capacity = body.get("node_capacity")
        if node_capacity and ((int(node_capacity) > max_nodes) or (int(node_capacity) < 1)):
            print(f'Access denied to {username}, node capacity: {node_capacity}, max nodes: {max_nodes}')
            return {
                'statusCode': 403,
                'body': json.dumps({'error': 'access denied'})
            }

        # check ttl
        ttl = body.get("ttl")
        if ttl and ((int(ttl) > max_ttl) or (int(ttl) < manager.MIN_TTL_FOR_AUTOSHUTDOWN)):
            print(f'Access denied to {username}, invalid ttl: {ttl}')
            return {
                'statusCode': 403,
                'body': json.dumps({'error': 'access denied'})
            }

        # check report and test runner
        if path.startswith('/report') or path.startswith('/test'):
            if 'owner' not in org_roles:
                print(f'Access denied to {username}, organization roles: {org_roles}, path: {path}')
                return {
                    'statusCode': 403,
                    'body': json.dumps({'error': 'access denied'})
                }

        # route request
        if path == '/deploy':
            return deploy_handler(body, context)
        elif path == '/extend':
            return extend_handler(body, context)
        elif path == '/destroy':
            return manager.lambda_destroy(body, context)
        elif path == '/status':
            return status_handler(body, context)
        elif path == '/events':
            return events_handler(body, context)
        elif path == '/report/clusters' or path == '/report':
            return report_clusters_handler(body, context)
        elif path == '/report/tests':
            return report_tests_handler(body, context)
        elif path == '/test':
            return test_handler(body, context)
        else:
            print(f'Path not found: {path}')
            return {
                'statusCode': 404,
                'body': json.dumps({'error': 'not found'})
            }

    except Exception as e:
        print(f'Exception in gateway: {e}')
        return {
            'statusCode': 500,
            'body': json.dumps({'error': 'internal error'})
        }
