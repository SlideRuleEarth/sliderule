import boto3
import json
import urllib3
from datetime import datetime, timedelta

# ###################
# Globals
# ###################

SUCCESS = "SUCCESS"
FAILED = "FAILED"

http = urllib3.PoolManager()

# ###################
# Utilities
# ###################

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

# ###################
# Lambda: Deleter
# ###################

def lambda_deleter(event, context):
    stack_name = event['stack_name']
    cf_client = boto3.client('cloudformation')
    events_client = boto3.client('events')

    print(f'Deleting stack: {stack_name}')

    try:
        # Clean up EventBridge rule BEFORE deleting the stack
        rule_name = f'{stack_name}-auto-delete'

        try:
            print(f'Removing targets from rule: {rule_name}')
            events_client.remove_targets(
                Rule=rule_name,
                Ids=['1']
            )
            print(f'Successfully removed targets from rule: {rule_name}')
        except events_client.exceptions.ResourceNotFoundException:
            print(f'Rule {rule_name} not found or no targets, skipping target removal')
        except Exception as e:
            print(f'Error removing targets: {e}')

        try:
            print(f'Deleting rule: {rule_name}')
            events_client.delete_rule(Name=rule_name)
            print(f'Successfully deleted rule: {rule_name}')
        except events_client.exceptions.ResourceNotFoundException:
            print(f'Rule {rule_name} not found, skipping deletion')
        except Exception as e:
            print(f'Error deleting rule: {e}')

        # Delete the stack
        cf_client.delete_stack(StackName=stack_name)
        print(f'Successfully initiated deletion of stack: {stack_name}')
        return {
            'statusCode': 200,
            'body': json.dumps(f'Stack {stack_name} deletion initiated')
        }
    except Exception as e:
        print(f'Error deleting stack: {str(e)}')
        raise

# ###################
# Lambda: Scheduler
# ###################

def lambda_scheduler(event, context):
    print(f'Event: {json.dumps(event)}')

    try:
        if event['RequestType'] == 'Delete':
            # Don't schedule deletion if stack is already being deleted
            print('Stack is being deleted, skipping scheduler')
            cfn_send(event, context, SUCCESS, {})
            return

        if event['RequestType'] in ['Create', 'Update']:
            stack_name = event['ResourceProperties']['StackName']
            delete_after_minutes = int(event['ResourceProperties']['DeleteAfterMinutes'])
            deletion_lambda_arn = event['ResourceProperties']['DeletionLambdaArn']

            events_client = boto3.client('events')
            lambda_client = boto3.client('lambda')

            # Calculate schedule time
            schedule_time = datetime.utcnow() + timedelta(minutes=delete_after_minutes)
            cron_expression = f'cron({schedule_time.minute} {schedule_time.hour} {schedule_time.day} {schedule_time.month} ? {schedule_time.year})'

            rule_name = f'{stack_name}-auto-delete'

            # Create EventBridge rule
            events_client.put_rule(
                Name=rule_name,
                ScheduleExpression=cron_expression,
                State='ENABLED',
                Description=f'Auto-delete stack {stack_name}'
            )

            # Add Lambda permission
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

            # Add target
            events_client.put_targets(
                Rule=rule_name,
                Targets=[{
                    'Id': '1',
                    'Arn': deletion_lambda_arn,
                    'Input': json.dumps({'stack_name': stack_name})
                }]
            )

            print(f'Scheduled stack deletion at {schedule_time} UTC')

            response_data = {
                'ScheduledTime': schedule_time.isoformat(),
                'RuleName': rule_name
            }

            cfn_send(event, context, SUCCESS, response_data)

    except Exception as e:
        print(f'Error: {str(e)}')
        cfn_send(event, context, FAILED, {'Error': str(e)})

# ###################
# Lambda: Run
# ###################

def lambda_run(event, context):

    # initialize response status
    status = {"Status": True}

    try:

        # build parameters for stack creation
        stackParameters = [
            {"ParameterKey": "Version", "ParameterValue": event["Version"]},
            {"ParameterKey": "Domain", "ParameterValue": event["Domain"]},
            {"ParameterKey": "ProjectBucket", "ParameterValue": event["ProjectBucket"]},
            {"ParameterKey": "ProjectFolder", "ParameterValue": event["ProjectFolder"]},
            {"ParameterKey": "LambdaZipFile", "ParameterValue": event["LambdaZipFile"]},
            {"ParameterKey": "ContainerRegistry", "ParameterValue": event["ContainerRegistry"]},
            {"ParameterKey": "DeployDate", "ParameterValue": event["DeployDate"]}
        ]

        # read template
        templateBody = open("testrunner.yml").read()

        # default to hardcoded stack name so only one can run at a time
        stackName = event.get('StackName', 'testrunner')
        status["StackName"] = stackName

        # create stack
        cf = boto3.client("cloudformation", region_name=event["Region"])
        response = cf.create_stack(StackName=stackName, TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=stackParameters)
        status["Response"] = response

    except Exception as e:

        # handle exceptions (return to user for debugging)
        status["Exception"] = f'{e}'
        status["Status"] = False

    # return status
    return status
