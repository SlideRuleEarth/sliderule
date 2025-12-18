import os
import boto3
import json
import base64
from datetime import datetime, timedelta
import scheduler

# ###############################
# Globals
# ###############################

MIN_TTL_FOR_AUTOSHUTDOWN = 15 # minutes

# ###############################
# Utilities
# ###############################

#
# Convention for deriving stack name from cluster
#
def build_stack_name(cluster):
    return f'{cluster}-cluster'


# ###############################
# Lambda: Deploy Cluster
# ###############################

def lambda_deploy(event, context):

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
        lambda_zip_file = os.environ['LAMBDA_ZIP_FILE']

        # get required request variables
        cluster = event["cluster"]
        is_public = event["is_public"]
        node_capacity = event["node_capacity"]
        ttl = event["ttl"]

        # get optional request variables
        version = event.get("version", "latest")
        organization = event.get("organization", cluster)
        region = event.get("region", "us-west-2")

        # get arn for auto-shutdown
        cf = boto3.client("cloudformation", region_name=region)
        resp = cf.describe_stacks(StackName=stack_name)
        outputs = resp["Stacks"][0].get("Outputs", [])
        destroy_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "DestroyLambdaArn")

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Version", "ParameterValue": version},
            {"ParameterKey": "IsPublic", "ParameterValue": str(is_public)},
            {"ParameterKey": "Organization", "ParameterValue": organization},
            {"ParameterKey": "Cluster", "ParameterValue": cluster},
            {"ParameterKey": "NodeCapacity", "ParameterValue": str(node_capacity)},
            {"ParameterKey": "TTL", "ParameterValue": str(ttl)},
            {"ParameterKey": "EnvironmentVersion", "ParameterValue": environment_version},
            {"ParameterKey": "Domain", "ParameterValue": domain},
            {"ParameterKey": "ProjectBucket", "ParameterValue": project_bucket},
            {"ParameterKey": "ProjectFolder", "ParameterValue": project_folder},
            {"ParameterKey": "ProjectPublicBucket", "ParameterValue": project_public_bucket},
            {"ParameterKey": "DestroyLambdaArn", "ParameterValue": destroy_lambda_arn},
            {"ParameterKey": "ContainerRegistry", "ParameterValue": container_registry},
            {"ParameterKey": "ProvisionerLambdaZipFile", "ParameterValue": lambda_zip_file},
        ]

        # read template
        templateBody = open("cluster.yml").read()

        # the stack name naming convention is required by Makefile
        state["stack_name"] = build_stack_name(cluster)

        # check rules for valid deployment
        if ttl >= 0 and ttl < MIN_TTL_FOR_AUTOSHUTDOWN:
            raise RuntimeError(f'Invalid TTL of {ttl} minutes, must be at least {MIN_TTL_FOR_AUTOSHUTDOWN} minutes to guarantee scheduled deletion')

        # create stack
        state["response"] = cf.create_stack(StackName=state["stack_name"], TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])
        print(f"Deploy initiated for {state["stack_name"]}")

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f"Exception in deploy: {e}")
        state["exception"] = f'{e}'
        state["status"] = False

    # return response
    return state

# ###############################
# Lambda: Extend Cluster
# ###############################

def lambda_extend(event, context):

    # initialize response state
    state = {"status": True}

    try:

        # get required request variables
        cluster = event["cluster"]
        ttl = event["ttl"]

        # get rule name
        rule_name = f'{build_stack_name(cluster)}-auto-shutdown'

        # calculate new shutdown time
        new_shutdown_time = datetime.utcnow() + timedelta(minutes=ttl)
        state["cron_expression"] = f'cron({new_shutdown_time.minute} {new_shutdown_time.hour} {new_shutdown_time.day} {new_shutdown_time.month} ? {new_shutdown_time.year})'

        # check rules for valid extension
        if ttl >= 0 and ttl < MIN_TTL_FOR_AUTOSHUTDOWN:
            raise RuntimeError(f'Invalid TTL of {ttl} minutes, must be at least {MIN_TTL_FOR_AUTOSHUTDOWN} minutes to guarantee scheduled deletion')

        # extend rule
        events = boto3.client('events')
        state["response"] = events.put_rule(
            Name=rule_name,
            ScheduleExpression=state["cron_expression"],
            State='ENABLED',
            Description=f"Automatic shutdown for {rule_name} (updated)"
        )

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f"Exception in extend: {e}")
        state["exception"] = f'{e}'
        state["status"] = False

    # return response
    return state

# ###############################
# Lambda: Destroy Cluster
# ###############################

def lambda_destroy(event, context):

    # initialize response status
    state = {"status": True}

    try:
        # get optional request variables
        cluster = event.get("cluster") # schedule deletions do not supply cluster parameter
        state["stack_name"] = event.get("stack_name", build_stack_name(cluster)) # scheduled deletions pass stack name
        region = event.get("region", "us-west-2")

        # delete eventbridge target and rule
        events = boto3.client("events")
        rule_name = f"{state["stack_name"]}-auto-shutdown"
        print(f'Delete initiated for {rule_name}')
        try:
            events.remove_targets(Rule=rule_name, Ids=["1"])
            state["EventBridge Target Removed"] = True
        except Exception as e:
            print(f'Unable to delete eventbridge rule: {e}')
            state["EventBridge Target Removed"] = False
        try:
            events.delete_rule(Name=rule_name, Force=False)
            state["EventBridge Rule Removed"] = True
        except Exception as e:
            print(f'Unable to delete eventbridge rule: {e}')
            state["EventBridge Rule Removed"] = False

        # delete stack
        cf = boto3.client("cloudformation", region_name=region)
        state["response"] = cf.delete_stack(StackName=state["stack_name"])
        print(f"Delete initiated for {state["stack_name"]}")

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f"Exception in destroy: {e}")
        state["exception"] = f'{e}'
        state["status"] = False

    # return response
    return state

# ###############################
# Lambda: Status Cluster
# ###############################

def lambda_status(event, context):

    # initialize response state
    state = {"status": True}

    try:
        # get required request variables
        cluster = event["cluster"]

        # get optional request variables
        region = event.get("region", "us-west-2")

        # get stack name
        state["stack_name"] = build_stack_name(cluster)

        # status stack
        print(f"Status requested for {state["stack_name"]}")
        cf = boto3.client("cloudformation", region_name=region)
        description = cf.describe_stacks(StackName=state["stack_name"])
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
        rule_name = scheduler.build_rule_name(state["stack_name"])
        state["auto_shutdown"] = scheduler.get_next_trigger_time(rule_name)

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f"Exception in status: {e}")
        state["exception"] = f'{e}'
        state["status"] = False

    # return response
    return state

# ###############################
# Lambda: Dump Cluster Logs
# ###############################

def lambda_events(event, context):

    # initialize response state
    state = {"status": True}

    try:
        # get required request variables
        cluster = event["cluster"]

        # get optional request variables
        region = event.get("region", "us-west-2")

        # get stack name
        state["stack_name"] = build_stack_name(cluster)

        # get events for stack
        cf = boto3.client("cloudformation", region_name="us-west-2")
        description = cf.describe_stack_events(StackName="developers-cluster")
        stack_events = description["StackEvents"]
        print(f"Events requested for {state["stack_name"]}")

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

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f"Exception in events: {e}")
        state["exception"] = f'{e}'
        state["status"] = False

    # return response
    return state

# ###############################
# Lambda: Test Runner
# ###############################

def lambda_test(event, context):

    # initialize response status
    state = {"status": True}

    try:
        # get require parameters
        version = event["version"]
        domain = event["domain"]
        project_bucket = event["project_bucket"]
        project_folder = event["project_folder"]
        lambda_zip_file = event["lambda_zip_file"]
        container_registry = event["container_registry"]
        deploy_date = event["deploy_date"]
        region = event["region"]

        # get optional parameters
        state["stack_name"] = event.get('stack_name', 'testrunner') # default to hardcoded stack name so only one can run at a time

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Version", "ParameterValue": version},
            {"ParameterKey": "Domain", "ParameterValue": domain},
            {"ParameterKey": "ProjectBucket", "ParameterValue": project_bucket},
            {"ParameterKey": "ProjectFolder", "ParameterValue": project_folder},
            {"ParameterKey": "LambdaZipFile", "ParameterValue": lambda_zip_file},
            {"ParameterKey": "ContainerRegistry", "ParameterValue": container_registry},
            {"ParameterKey": "DeployDate", "ParameterValue": deploy_date}
        ]

        # read template
        templateBody = open("testrunner.yml").read()

        # copy test runner to S3 (where cloudformation can find it)
        s3 = boto3.client("s3")
        s3.upload_file(Filename="testrunner.sh", Bucket=project_bucket, Key=f"{project_folder}/testrunner.sh")

        # create stack
        cf = boto3.client("cloudformation", region_name=region)
        state["response"] = cf.create_stack(StackName=state["stack_name"], TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

    except Exception as e:

        # handle exceptions (return to user for debugging)
        print(f"Exception in test: {e}")
        state["exception"] = f'{e}'
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
    path = event.get('rawPath', '')
    body_raw = event.get("body")
    if body_raw:
        if event.get("isBase64Encoded"):
            body_raw = base64.b64decode(body_raw).decode("utf-8")
        body = json.loads(body_raw)
    else:
        body = {}

    print(f"Received request: {path} {body}")
    if path == '/deploy':
        return lambda_deploy(body, context)
    elif path == '/extend':
        return lambda_extend(body, context)
    elif path == '/destroy':
        return lambda_destroy(body, context)
    elif path == '/status':
        return lambda_status(body, context)
    elif path == '/events':
        return lambda_events(body, context)
    elif path == '/test':
        return lambda_test(body, context)
    else:
        print(f"Path not found: {path}")
        return {
            'statusCode': 404,
            'body': json.dumps({'error': 'not found'})
        }
