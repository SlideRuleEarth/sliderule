import os
import json
import base64
import boto3
import hashlib
import botocore.exceptions
from datetime import datetime, timezone
from cryptography.hazmat.primitives.serialization import load_ssh_public_key

# ###############################
# Globals
# ###############################

DOMAIN = os.environ["DOMAIN"]
STACK_NAME = os.environ["STACK_NAME"]
ENVIRONMENT_VERSION = os.environ['ENVIRONMENT_VERSION']
PROJECT_PUBLIC_BUCKET = os.environ["PROJECT_PUBLIC_BUCKET"]
SUPPORT_EMAIL = os.environ['SUPPORT_EMAIL']
ALERT_EMAIL = os.environ['ALERT_EMAIL']

JOB_STATES = ["SUBMITTED", "PENDING", "RUNNABLE", "STARTING", "RUNNING", "SUCCEEDED", "FAILED"]
MAX_JOBS_TO_DESCRIBE = 100
MAX_VCPUS = 8
MIN_VCPUS = 1
MAX_MEMORY = 32
MIN_MEMORY = 8

batch = boto3.client("batch")
ses = boto3.client('ses')
s3 = boto3.client("s3")
sm = boto3.client('secretsmanager')

_pubkey_cache = {}

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
    response = ses.send_email(
        Source = SUPPORT_EMAIL,
        Destination = {
            "ToAddresses": [ALERT_EMAIL]
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
        print(f"Error sending email from {SUPPORT_EMAIL} to {ALERT_EMAIL}: {response}")
        return False

#
# Verify Signature (on Request)
#
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
            secret_name = f"{DOMAIN}/pubkeys"
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

# ###############################
# Path Handlers
# ###############################

#
# Submit Job
#
def submit_handler(body, username):

    # initialize response state
    state = {}

    # get required request variables
    name = body["name"]
    script = base64.b64decode(body["script"]).decode('utf-8')
    args_list = body["args_list"]

    # get optional request variables
    vcpus = body.get("vcpus")
    memory = body.get("memory")

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
    run_path = f"{STACK_NAME}/{run_id}"
    run_url = f"s3://{PROJECT_PUBLIC_BUCKET}/{run_path}"

    # populate validated initial info
    state["name"] = name
    state["run_url"] = run_url

    # load initial files to S3
    s3.put_object(Bucket=PROJECT_PUBLIC_BUCKET, Key=f"{run_path}/script.lua", Body=script)
    s3.put_object(Bucket=PROJECT_PUBLIC_BUCKET, Key=f"{run_path}/receipt.json", Body=json.dumps({
        "name": name,
        "username": username,
        "args": {str(i):args_list[i] for i in range(len(args_list))},
        "environment": ENVIRONMENT_VERSION
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
            jobQueue=f"{STACK_NAME}-job-queue",
            jobDefinition=f"{STACK_NAME}-default-job-definition",
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

    # status deployment
    send_email(f"SlideRule Job Submission (@{username} {name})", json.dumps(state, indent=2))
    print(f'Jobs submitted on {STACK_NAME}: {len(job_ids)}')

    # success
    return json_response(200, state)

#
# Job Report
#
def report_jobs_handler(body):

    # initialize response state
    state = {}

    # get required request variables
    job_list = body["job_list"]

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
            state[job_id] = {
                "status": job["status"],
                "statusReason": job.get("statusReason", "n/a"),
                "createdAt": datetime.fromtimestamp(job["createdAt"] / 1000, tz=timezone.utc).strftime("%Y-%m-%d %H:%M:%S"),
                "jobDefinition": job["jobDefinition"],
                "args": job["parameters"]['args']
            }

    # success
    return json_response(200, state)

#
# Job Queue Report
#
def report_queue_handler(body):

    # initialize response state
    state = {"report": {}, "jobs": []}

    # get required request variables
    job_state = body["job_state"]

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
            jobQueue=f"{STACK_NAME}-job-queue",
            jobStatus=job_status
        )
        state["report"][job_status] = len(response["jobSummaryList"])
        for job in response["jobSummaryList"]:
            state["jobs"].append({"job_id": job["jobId"], "name": job["jobName"], "status": job["status"]})

    # success
    return json_response(200, state)

#
# Job Cancel
#
def cancel_handler(body):

    # initialize response state
    state = []

    # get optional request variables
    job_list = body.get("job_list")

    # get jobs to delete
    jobs_to_delete = job_list
    if jobs_to_delete == None:
        # find all jobs
        for state in ["SUBMITTED", "PENDING", "RUNNABLE"]:
            response = batch.list_jobs(
                jobQueue=f"{STACK_NAME}-job-queue",
                jobStatus=state
            )
            for job in response["jobSummaryList"]:
                jobs_to_delete.append(job["jobId"])

    # delete jobs
    for job_id in jobs_to_delete:
        batch.cancel_job(jobId=job_id, reason="Canceled by user")
        state.append(job_id)

    # success
    return json_response(200, state)

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
        username = claims.get('sub', '<anonymous>')
        org_roles = parse_claim_array(claims.get('org_roles', "[]"))
        path = event.get('rawPath', '')
        body_raw = event.get("body")
        if body_raw:
            if event.get("isBase64Encoded"):
                body_raw = base64.b64decode(body_raw).decode("utf-8")
            body = json.loads(body_raw)
        else:
            body = {}
        print(f'Received request: {path} {body}')

        # check signature (for all requests)
        if not verify_signature(path, body_raw, username, event):
            return json_response(403, {'error': 'access denied', 'error_description': 'invalid signature'})

        # check organization membership
        if 'member' not in org_roles:
            return json_response(403, {'error': 'access denied', 'error_description': 'not a member'})

        # route request
        if path == '/submit': # submits batch runner job
            return submit_handler(body, username)
        elif path == '/report/jobs': # returns status report on batch runner jobs
            return report_jobs_handler(body)
        elif path == '/report/queue': # returns report on all the jobs submitted
            return report_queue_handler(body)
        elif path == '/cancel': # cancel a submitted job
            return cancel_handler(body)

        # invalid path
        return json_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return exception_reponse(e)
