import os
import boto3
import json
import urllib3
from datetime import datetime, timedelta

# ###############################
# Globals
# ###############################

MIN_TTL_FOR_AUTOSHUTDOWN = 15 # minutes

SUCCESS = "SUCCESS"
FAILED = "FAILED"

http = urllib3.PoolManager()

# ###############################
# Utilities
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
        response = http.request('PUT', responseUrl, headers=headers, body=json_responseBody)
        print("Status code:", response.status)
    except Exception as e:
        print("send(..) failed executing http.request(..):", e)

#
# Convention for deriving stack name from cluster
#
def build_stack_name(cluster):
    return f'{cluster}-cluster'

# ###############################
# Lambda: Schedule Auto Shutdown
# ###############################

def lambda_scheduler(event, context):
    print(f'Event: {json.dumps(event)}')

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
            rule_name = f'{stack_name}-auto-shutdown'

            # get clients
            events_client = boto3.client('events')
            lambda_client = boto3.client('lambda')

            # calculate schedule time
            shutdown_time = datetime.utcnow() + timedelta(minutes=delete_after_minutes)
            cron_expression = f'cron({shutdown_time.minute} {shutdown_time.hour} {shutdown_time.day} {shutdown_time.month} ? {shutdown_time.year})'

            # create eventbridge rule
            events_client.put_rule(
                Name=rule_name,
                ScheduleExpression=cron_expression,
                State='ENABLED',
                Description=f'Automatically shutdown stack {stack_name}'
            )

            # add lambda permission
            try:
                lambda_client.add_permission(
                    FunctionName=deletion_lambda_arn,
                    StatementId=f'{rule_name}-permission',
                    Action='lambda:InvokeFunction',
                    Principal='events.amazonaws.com',
                    SourceArn=f'arn:aws:events:{context.invoked_function_arn.split(":")[3]}:{context.invoked_function_arn.split(":")[4]}:rule/{rule_name}'
                )
            except lambda_client.exceptions.ResourceConflictException:
                print('Permission already exists')

            # add target
            events_client.put_targets(
                Rule=rule_name,
                Targets=[{
                    'Id': '1',
                    'Arn': deletion_lambda_arn,
                    'Input': json.dumps({'StackName': stack_name})
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
# Lambda: Deploy Cluster
# ###############################

def lambda_deploy(event, context):

    # initialize response state
    state = {"Status": True}

    try:
        # get environment variables
        environment_version = os.environ['ENVIRONMENT_VERSION']
        domain = os.environ["DOMAIN"]
        project_bucket = os.environ["PROJECT_BUCKET"]
        project_folder = os.environ["PROJECT_FOLDER"]
        project_public_bucket = os.environ["PROJECT_PUBLIC_BUCKET"]
        container_registry = os.environ['CONTAINER_REGISTRY']
        lambda_zip_file = os.environ['LAMBDA_ZIP_FILE']

        # get required request variables
        cluster = event["Cluster"]
        is_public = event["IsPublic"]
        node_capacity = event["NodeCapacity"]
        ttl = event["TTL"]

        # get optional request variables
        version = event.get("Version", "latest")
        organization = event.get("Organization", cluster)
        region = event.get("Region", "us-west-2")
        dryrun = event.get("DryRun", False)

        # build parameters for stack creation
        state["StackParameters"] = [
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
            {"ParameterKey": "ContainerRegistry", "ParameterValue": container_registry},
            {"ParameterKey": "ProvisionerLambdaZipFile", "ParameterValue": lambda_zip_file},
        ]

        # read template
        templateBody = open("cluster.yml").read()

        # the stack name naming convention is required by Makefile
        state["StackName"] = build_stack_name(cluster)

        # check rules for valid deployment
        if ttl >= 0 and ttl < MIN_TTL_FOR_AUTOSHUTDOWN:
            raise RuntimeError(f'Invalid TTL of {ttl} minutes, must be at least {MIN_TTL_FOR_AUTOSHUTDOWN} minutes to guarantee scheduled deletion')

        # create stack
        cf = boto3.client("cloudformation", region_name=region)
        if not dryrun:
            state["Response"] = cf.create_stack(StackName=state["StackName"], TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["StackParameters"])
        print(f"Deploy initiated for {state["StackName"]}")

    except Exception as e:

        # handle exceptions (return to user for debugging)
        state["Exception"] = f'{e}'
        state["Status"] = False

    # return response
    return state

# ###############################
# Lambda: Extend Cluster
# ###############################

def lambda_extend(event, context):

    # initialize response state
    state = {"Status": True}

    try:

        # get required request variables
        cluster = event["Cluster"]
        ttl = event["TTL"]

        # get rule name
        rule_name = f'{build_stack_name(cluster)}-auto-shutdown'

        # calculate new shutdown time
        new_shutdown_time = datetime.utcnow() + timedelta(minutes=ttl)
        state["CronExpression"] = f'cron({new_shutdown_time.minute} {new_shutdown_time.hour} {new_shutdown_time.day} {new_shutdown_time.month} ? {new_shutdown_time.year})'

        # check rules for valid extension
        if ttl >= 0 and ttl < MIN_TTL_FOR_AUTOSHUTDOWN:
            raise RuntimeError(f'Invalid TTL of {ttl} minutes, must be at least {MIN_TTL_FOR_AUTOSHUTDOWN} minutes to guarantee scheduled deletion')

        # extend rule
        events = boto3.client('events')
        state["Response"] = events.put_rule(
            Name=rule_name,
            ScheduleExpression=state["CronExpression"],
            State='ENABLED',
            Description=f"Automatic shutdown for {rule_name} (updated)"
        )

    except Exception as e:

        # handle exceptions (return to user for debugging)
        state["Exception"] = f'{e}'
        state["Status"] = False

    # return response
    return state

# ###############################
# Lambda: Destroy Cluster
# ###############################

def lambda_destroy(event, context):

    # initialize response status
    state = {"Status": True}

    try:
        # get required request variables
        cluster = event["Cluster"]

        # get optional request variables
        state["StackName"] = event.get("StackName", build_stack_name(cluster)) # scheduled deletions pass stack name
        region = event.get("Region", "us-west-2")

        # delete eventbridge target and rule
        events = boto3.client("events")
        rule_name = f"{state["StackName"]}-auto-shutdown"
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
        state["Response"] = cf.delete_stack(StackName=state["StackName"])
        print(f"Delete initiated for {state["StackName"]}")

    except Exception as e:

        # handle exceptions (return to user for debugging)
        state["Exception"] = f'{e}'
        state["Status"] = False

    # return response
    return state

# ###############################
# Lambda: Status Cluster
# ###############################

def lambda_status(event, context):

    # initialize response state
    state = {"Status": True}

    try:
        # get required request variables
        cluster = event["Cluster"]

        # get optional request variables
        region = event.get("Region", "us-west-2")

        # get stack name
        state["StackName"] = build_stack_name(cluster)

        # status stack
        cf = boto3.client("cloudformation", region_name=region)
        description  = cf.describe_stacks(StackName=state["StackName"])
        stack = description["Stacks"][0]
        print(f"Status requested for {state["StackName"]}")

        # build cleaned response
        response = {}
        for k, v in stack.items():
            if hasattr(v, "tolist"):
                response[k] = v.tolist()
            elif isinstance(v, (datetime)):
                response[k] = v.isoformat()
            elif not isinstance(v, (bytes, bytearray)):
                response[k] = v
        state["Response"] = response

    except Exception as e:

        # handle exceptions (return to user for debugging)
        state["Exception"] = f'{e}'
        state["Status"] = False

    # return response
    return state

# ###############################
# Lambda: Dump Cluster Logs
# ###############################

def lambda_events(event, context):

    # initialize response state
    state = {"Status": True}

    try:
        # get required request variables
        cluster = event["Cluster"]

        # get optional request variables
        region = event.get("Region", "us-west-2")

        # get stack name
        state["StackName"] = build_stack_name(cluster)

        # get events for stack
        cf = boto3.client("cloudformation", region_name="us-west-2")
        description = cf.describe_stack_events(StackName="developers-cluster")
        stack_events = description["StackEvents"]
        print(f"Events requested for {state["StackName"]}")

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
        state["Response"] = response

    except Exception as e:

        # handle exceptions (return to user for debugging)
        state["Exception"] = f'{e}'
        state["Status"] = False

    # return response
    return state

# ###############################
# Lambda: Test Runner
# ###############################

def lambda_test(event, context):

    # initialize response status
    state = {"Status": True}

    try:
        # get require parameters
        version = event["Version"]
        domain = event["Domain"]
        project_bucket = event["ProjectBucket"]
        project_folder = event["ProjectFolder"]
        lambda_zip_file = event["LambdaZipFile"]
        container_registry = event["ContainerRegistry"]
        deploy_date = event["DeployDate"]
        region = event["Region"]

        # get optional parameters
        state["StackName"] = event.get('StackName', 'testrunner') # default to hardcoded stack name so only one can run at a time

        # build parameters for stack creation
        state["StackParameters"] = [
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
        state["Response"] = cf.create_stack(StackName=state["StackName"], TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["StackParameters"])

    except Exception as e:

        # handle exceptions (return to user for debugging)
        state["Exception"] = f'{e}'
        state["Status"] = False

    # return response
    return state
