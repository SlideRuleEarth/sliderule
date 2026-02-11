import os
import boto3
import json
import base64
import hashlib
import botocore.exceptions
from datetime import datetime, timezone

# ###############################
# Cached Clients
# ###############################

s3 = boto3.client("s3")
batch = boto3.client("batch")

MAX_JOBS_TO_DESCRIBE = 100
JOB_STATES = ["SUBMITTED", "PENDING", "RUNNABLE", "STARTING", "RUNNING", "SUCCEEDED", "FAILED"]

# ###############################
# Utilities
# ###############################

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
# Submit Job
#
def submit_handler(event, context):

    # initialize response state
    state = {"status": True}

    try:
        # get environment variables
        stack_name = os.environ["STACK_NAME"]
        environment_version = os.environ['ENVIRONMENT_VERSION']
        project_public_bucket = os.environ["PROJECT_PUBLIC_BUCKET"]

        # get required request variables
        name = event["name"]
        script = base64.b64decode(event["script"]).decode('utf-8')
        args_list = event["args_list"]

        # parameter validation
        if not isinstance(name, str):
            raise RuntimeError(f"Invalid name supplied of type {type(name)}")
        elif len(script) <= 0:
            raise RuntimeError(f"Empty script provided")
        elif not isinstance(args_list, list):
            raise RuntimeError(f"Invalid array of arguments supplied of type {type(args_list)}")

        # build unique identifier
        now = datetime.now(timezone.utc).isoformat()
        script_hash = hashlib.sha256(script.encode('utf-8')).hexdigest()
        combined = now + name + script_hash
        final_hash = hashlib.sha256(combined.encode('utf-8')).hexdigest()[:10]
        run_id = f"run-{final_hash}"
        s3_run_path = f"{stack_name}/{run_id}"

        # load initial files to S3
        s3.put_object(Bucket=project_public_bucket, Key=f"{s3_run_path}/environment.txt", Body=environment_version)
        s3.put_object(Bucket=project_public_bucket, Key=f"{s3_run_path}/script.lua", Body=script)

        # submit jobs
        job_ids = []
        for args in args_list:
            response = batch.submit_job(
                jobName=name,
                jobQueue=f"{stack_name}-job-queue",
                jobDefinition=f"{stack_name}-default-job-definition",
                parameters={
                    "script": f"s3://{project_public_bucket}/{s3_run_path}/script.lua",
                    "args": args,
                    "result": f"s3://{project_public_bucket}/{s3_run_path}/result.json"
                }
            )
            job_ids.append(response["jobId"])
            print(f'Job <{name}> submitted, aws batch job id = {response["jobId"]}, sliderule runner run id = {run_id}')

        # populate state
        state["name"] = name
        state["job_ids"] = job_ids
        state["run_id"] = run_id

    except botocore.exceptions.ClientError as e:
        print(f'Failed to submit job: {e}')
        state["exception"] = f'Failed to submit job'
        state["status"] = False

    except RuntimeError as e:
        print(f'User error in job submission: {e}')
        state["exception"] = f'User error in job submission'
        state["status"] = False

    except Exception as e:
        print(f'Exception in job submission: {e}')
        state["exception"] = f'Failure in job submission'
        state["status"] = False

    # return response
    return state

#
# Job Report
#
def report_jobs_handler(event, context):

    # initialize response state
    state = {"status": True, "report": {}}

    try:

        # get required request variables
        job_list = event["job_list"]

        # parameter validation
        if not isinstance(job_list, list):
            raise RuntimeError(f"Job list must be supplied as a list: {type(job_list)}")
        elif len(job_list) > MAX_JOBS_TO_DESCRIBE:
            raise RuntimeError(f"Can only status up to {MAX_JOBS_TO_DESCRIBE} at a time")

        # describe jobs
        response = batch.describe_jobs(jobs=job_list)
        for i in range(len(job_list)):
            job = response["jobs"][i]
            state["report"][job_list[i]] = {
                "status": job["status"],
                "statusReason": job["status"] == "FAILED" and job.get("statusReason") or "n/a",
                "createdAt": job["createdAt"],
                "startedAt": job["startedAt"],
                "stoppedAt": job["stoppedAt"]
            }

    except RuntimeError as e:
        print(f'User error in job report: {e}')
        state["exception"] = f'User error in job report'
        state["status"] = False

    except Exception as e:
        print(f'Exception in job report: {e}')
        state["exception"] = f'Failure in job report'
        state["status"] = False

    # return response
    return state

#
# Job Queue Report
#
def report_queue_handler(event, context):

    # initialize response state
    state = {"status": True, "report": {}}

    try:

        # get environment variables
        stack_name = os.environ["STACK_NAME"]

        # get required request variables
        job_state = event["job_state"]

        # get job states list
        job_states = None
        if isinstance(job_state, list):
            job_states = job_state
        elif isinstance(job_state, str):
            job_states = [job_state]
        else:
            raise RuntimeError(f"Invalid job states supplied: {type(job_state)}")

        # validate parameters
        for state in job_states:
            if not isinstance(state, str):
                raise RuntimeError(f"Invalid job state supplied: {type(state)}")
            elif state not in JOB_STATES:
                raise RuntimeError(f"Unknown job state supplied: {state}")

        # list jobs
        job_summary = []
        for state in job_states:
            response = batch.list_jobs(
                jobQueue=f"{stack_name}-job-queue",
                jobStatus=state
            )
            for job in response["jobSummaryList"]:
                job_summary.append({"job_id": job["jobId"], "name": job["jobName"], "status": job["status"]})

    except RuntimeError as e:
        print(f'User error in queue report: {e}')
        state["exception"] = f'User error in queue report'
        state["status"] = False

    except Exception as e:
        print(f'Exception in queue report: {e}')
        state["exception"] = f'Failure in queue report'
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

        # route request
        if path == '/submit':
            return submit_handler(body, context)
        elif path == '/report/jobs':
            return report_jobs_handler(body, context)
        elif path == '/report/queue':
            return report_queue_handler(body, context)
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
