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

JOB_STATES = ["SUBMITTED", "PENDING", "RUNNABLE", "STARTING", "RUNNING", "SUCCEEDED", "FAILED"]
MAX_JOBS_TO_DESCRIBE = 100
MAX_VCPUS = 8
MIN_VCPUS = 1
MAX_MEMORY = 32
MIN_MEMORY = 8

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
def submit_handler(event, context, username):

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

        # get optional request variables
        vcpus = event.get("vcpus")
        memory = event.get("memory")

        # parameter validation
        if not isinstance(name, str):
            raise RuntimeError(f"Invalid name supplied of type {type(name)}")
        elif len(script) <= 0:
            raise RuntimeError(f"Empty script provided")
        elif not isinstance(args_list, list):
            raise RuntimeError(f"Invalid array of arguments supplied of type {type(args_list)}")
        elif (vcpus != None) and ((not isinstance(vcpus, int)) or (vcpus < MIN_VCPUS) or (vcpus > MAX_VCPUS)):
            raise RuntimeError(f"Invalid vCPUs provided: {vcpus}")
        elif (memory != None) and ((not isinstance(memory, int)) or (memory < MIN_MEMORY) or (memory > MAX_MEMORY)):
            raise RuntimeError(f"Invalid memory provided: {memory}")

        # build unique identifier
        now = datetime.now(timezone.utc).isoformat()
        script_hash = hashlib.sha256(script.encode('utf-8')).hexdigest()
        combined = now + name + script_hash
        final_hash = hashlib.sha256(combined.encode('utf-8')).hexdigest()[:10]
        clean_username = "".join(ch if ch.isalnum() else "_" for ch in username.split(" ")[0][:16])
        clean_name = "".join(ch if ch.isalnum() else "_" for ch in name.split(" ")[0][:16])
        run_id = f"run-{clean_username}-{clean_name}-{final_hash}"
        run_path = f"{stack_name}/{run_id}"
        run_url = f"s3://{project_public_bucket}/{run_path}"

        # populate validated initial info
        state["name"] = name
        state["run_url"] = run_url

        # load initial files to S3
        s3.put_object(Bucket=project_public_bucket, Key=f"{run_path}/script.lua", Body=script)
        s3.put_object(Bucket=project_public_bucket, Key=f"{run_path}/receipt.json", Body=json.dumps({
            "name": name,
            "username": username,
            "args": {str(i):args_list[i] for i in range(len(args_list))},
            "environment": environment_version
        }, indent=2))

        # optionally build container overrides
        container_overrides = {}
        if (vcpus != None) or (memory != None):
            container_overrides = {"resourceRequirements": []}
            if vcpus != None:
                container_overrides["resourceRequirements"].append({"type": "VCPU", "value": str(vcpus)})
            if memory != None:
                container_overrides["resourceRequirements"].append({"type": "MEMORY", "value": str(memory)})

        # submit jobs
        job_ids = []
        for i in range(len(args_list)):
            response = batch.submit_job(
                jobName=name,
                jobQueue=f"{stack_name}-job-queue",
                jobDefinition=f"{stack_name}-default-job-definition",
                parameters={
                    "script": f"{run_url}/script.lua",
                    "args": len(args_list[i].strip()) > 0 and args_list[i] or "nil",
                    "result": f"{run_url}/result_{i}.json"
                },
                containerOverrides=container_overrides
            )
            job_ids.append(response["jobId"])
            print(f'Job <{name}> submitted, aws batch job id = {response["jobId"]}, sliderule runner run id = {run_id}')
        state["job_ids"] = job_ids

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
        jobs_by_id = {job["jobId"]: job for job in response["jobs"]}
        for job_id in job_list:
            job = jobs_by_id.get(job_id)
            if job is not None:
                state["report"][job_id] = {
                    "status": job["status"],
                    "statusReason": job.get("statusReason", "n/a"),
                    "createdAt": datetime.fromtimestamp(job["createdAt"] / 1000, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
                    "jobDefinition": job["jobDefinition"],
                    "args": job["parameters"]['args']
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
    state = {"status": True, "report": {}, "jobs": []}

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
        for job_status in job_states:
            if not isinstance(job_status, str):
                raise RuntimeError(f"Invalid job state supplied: {type(job_status)}")
            elif job_status not in JOB_STATES:
                raise RuntimeError(f"Unknown job state supplied: {job_status}")

        # list jobs
        for job_status in job_states:
            response = batch.list_jobs(
                jobQueue=f"{stack_name}-job-queue",
                jobStatus=job_status
            )
            state["report"][job_status] = len(response["jobSummaryList"])
            for job in response["jobSummaryList"]:
                state["jobs"].append({"job_id": job["jobId"], "name": job["jobName"], "status": job["status"]})

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

#
# Job Cancel
#
def cancel_handler(event, context):

    # initialize response state
    state = {"status": True, "jobs_deleted": []}

    try:
        # get environment variables
        stack_name = os.environ["STACK_NAME"]

        # get optional request variables
        job_list = event.get("job_list")

        # get jobs to delete
        jobs_to_delete = job_list
        if jobs_to_delete == None:
            # find all jobs
            for state in ["SUBMITTED", "PENDING", "RUNNABLE"]:
                response = batch.list_jobs(
                    jobQueue=f"{stack_name}-job-queue",
                    jobStatus=state
                )
                for job in response["jobSummaryList"]:
                    jobs_to_delete.append(job["jobId"])

        # delete jobs
        for job_id in jobs_to_delete:
            batch.cancel_job(jobId=job_id, reason="Canceled by user")
            state["jobs_deleted"].append(job_id)

    except RuntimeError as e:
        print(f'User error in job deletion: {e}')
        state["exception"] = f'User error in job deletion'
        state["status"] = False

    except Exception as e:
        print(f'Exception in job deletion: {e}')
        state["exception"] = f'Failure in job deletion'
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
            return submit_handler(body, context, username)
        elif path == '/report/jobs':
            return report_jobs_handler(body, context)
        elif path == '/report/queue':
            return report_queue_handler(body, context)
        elif path == '/cancel':
            return cancel_handler(body, context)
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
