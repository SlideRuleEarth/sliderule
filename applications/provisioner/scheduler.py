import boto3
import json
import urllib3
from datetime import datetime, timedelta, timezone

# ###############################
# Globals
# ###############################

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
# Get the next time of the event bridge rule
#
def get_next_trigger_time(rule_name):
    """
    Return next trigger for cron() expressions in an EventBridge rule.
    Example: cron(0 12 * * ? *) - daily at noon UTC
    """
    client = boto3.client('events')

    try:
        from croniter import croniter

        # Get the rule details
        response = client.describe_rule(Name=rule_name)

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
    except client.exceptions.ResourceNotFoundException:
        print(f"Rule {rule_name} not found")
        return None
    except Exception as e:
        print(f"Error getting rule details: {e}")
        return None

#
# Convention for deriving rule name from stack name
#
def build_rule_name(stack_name):
    return f'{stack_name}-auto-shutdown'


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
            rule_name = build_rule_name(stack_name)

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

