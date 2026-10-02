import os
import re
import json
import base64
import boto3
import time
import hashlib
import secrets
import botocore.exceptions
from datetime import datetime, timezone
from concurrent.futures import ThreadPoolExecutor, as_completed
from cryptography.hazmat.primitives.serialization import load_ssh_public_key

# ###############################
# Globals
# ###############################

DOMAIN = os.environ["DOMAIN"]
STACK_NAME = os.environ["STACK_NAME"]
ENVIRONMENT_VERSION = os.environ['ENVIRONMENT_VERSION']
PROJECT_PUBLIC_BUCKET = os.environ["PROJECT_PUBLIC_BUCKET"]
SECRETS_TABLE = os.environ["SECRETS_TABLE"]
SUPPORT_EMAIL = os.environ['SUPPORT_EMAIL']
ALERT_EMAIL = os.environ['ALERT_EMAIL']
IMAGE_TAGS = [tag.strip() for tag in os.environ['IMAGE_TAGS'].split(",")]
PROTECTED_IMAGE_TAGS = [tag.strip() for tag in os.environ['PROTECTED_IMAGE_TAGS'].split(",")]

JOB_NAME = re.compile(r"[A-Za-z0-9][A-Za-z0-9_-]{0,127}")
JOB_STATES = ["SUBMITTED", "PENDING", "RUNNABLE", "STARTING", "RUNNING", "SUCCEEDED", "FAILED"]
JOB_QUEUES = ["urgent", "default", "background"]
MAX_JOBS_TO_DESCRIBE = 100
MAX_VCPUS = 8
MIN_VCPUS = 1
MAX_MEMORY = 32768
MIN_MEMORY = 4000
API_CONCURRENCY = 10
MAX_ARGS_ARRAY_SIZE = 10000
SECRET_EXPIRATION_HOURS = 72

batch = boto3.client("batch")
ses = boto3.client('ses')
s3 = boto3.client("s3")
sm = boto3.client('secretsmanager')
logs = boto3.client("logs")
dynamodb = boto3.resource("dynamodb")

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
def exception_response(e):
    print(f'Exception: {e}')
    if isinstance(e, botocore.exceptions.ClientError) and (e.response['Error']['Code'] == 'ValidationError'):
        return json_response(404, {'error': 'not found', 'error_description': 'failure in request'})
    return json_response(500, {'error': 'internal error', 'error_description': 'failure in request'})

#
# Send Email Message
#
def send_email(title, message):
    try:
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
        if response["ResponseMetadata"]["HTTPStatusCode"] < 200 or response["ResponseMetadata"]["HTTPStatusCode"] >= 300:
            raise RuntimeError(f"Failed to send email: {response}")
    except Exception as e:
        print(f"Error sending email from {SUPPORT_EMAIL} to {ALERT_EMAIL}: {e}")

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

#
# List Jobs
#
def list_jobs(job_states, queue, job_id):
    """
    create a list of jobs depending on the parameters
    ALL JOBS - if job_id is None
    CHILD JOBS - if job_id is a parent job
    SINGLE JOB - if job_id is a single job
    """
    # check for single job
    if job_id:
        job = batch.describe_jobs(jobs=[job_id]).get("jobs",[None])[0]
        is_singleton = "size" not in job.get("arrayProperties", {})
        if is_singleton:
            if job["status"] in job_states:
                return [job]
            else:
                return []

    # either list child jobs (job_id provided) or list all jobs (only queue name provided)
    job_list = []
    for job_status in job_states:
        if job_id:
            parms = { "arrayJobId": job_id, "jobStatus": job_status }
        else:
            parms = {"jobQueue": f"{STACK_NAME}-{queue}-job-queue", "jobStatus": job_status}
        while True:
            response = batch.list_jobs(**parms)
            job_list.extend(response["jobSummaryList"])
            next_token = response.get("nextToken")
            if not next_token:
                break
            parms["nextToken"] = next_token
    return job_list

#
# Cancel Jobs
#
def cancel_job(job_id):
    batch.cancel_job(jobId=job_id, reason="Canceled by user")
    return job_id

#
# Get Job Owner
#
def get_job_owner(job_id):
    """
    returns the user tag of the submitted job, or None if not found
    """
    if not isinstance(job_id, str):
        return None
    # array child job ids are "<parent id>:<index>" and the tag lives on the parent
    jobs = batch.describe_jobs(jobs=[job_id.split(":")[0]])["jobs"]
    if not jobs:
        return None
    return jobs[0].get("tags", {}).get("user")

# ###############################
# Path Handlers
# ###############################

#
# Submit Job
#
def submit_handler(body, username, secret_values):

    # initialize response state
    state = {}

    # get required request variables
    name = body["name"]
    script = base64.b64decode(body["script"]).decode('utf-8')
    args = body["args"]

    # get optional request variables
    image = body.get("image", "sliderule:latest")
    queue = body.get("queue", "default")
    vcpus = body.get("vcpus")
    memory = body.get("memory")

    # parameter validation
    if not isinstance(image, str):
        raise RuntimeError(f"Invalid image specified of type: {type(image)}")
    elif not isinstance(name, str):
        raise RuntimeError(f"Invalid name supplied of type {type(name)}")
    elif not JOB_NAME.fullmatch(name):
        raise RuntimeError(f"Invalid name supplied: {name}")
    elif len(script) <= 0:
        raise RuntimeError(f"Empty script provided")
    elif (not isinstance(args, list)) and (not isinstance(args, dict)) and (not isinstance(args, str)):
        raise RuntimeError(f"Invalid arguments type: {type(args)}")
    elif isinstance(args, list) and (len(args) > MAX_ARGS_ARRAY_SIZE):
        raise RuntimeError(f"Argument array size too large: {len(args)}")
    elif isinstance(args, list) and (len(args) == 0):
        raise RuntimeError(f"Argument array cannot be empty")
    elif (vcpus != None) and ((not isinstance(vcpus, int)) or (vcpus < MIN_VCPUS) or (vcpus > MAX_VCPUS)):
        raise RuntimeError(f"Invalid vCPUs provided: {vcpus}")
    elif (memory != None) and ((not isinstance(memory, int)) or (memory < MIN_MEMORY) or (memory > MAX_MEMORY)):
        raise RuntimeError(f"Invalid memory provided: {memory}")

    # sanitize and check image
    image = image.replace(":","-")
    if image not in IMAGE_TAGS:
        raise RuntimeError(f"Invalid image provided: {image}")

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
    args_path = f"{run_path}/args.json"
    args_url = f"s3://{PROJECT_PUBLIC_BUCKET}/{args_path}"
    script_path = f"{run_path}/script"
    script_url = f"s3://{PROJECT_PUBLIC_BUCKET}/{script_path}"

    # populate validated initial info
    state["name"] = name
    state["run_url"] = run_url

    # handle arguments
    process_as_array = isinstance(args, list) and len(args) > 1
    if process_as_array or isinstance(args, dict):
        args_contents = json.dumps(args)
    elif isinstance(args, list) and len(args) == 1:
        args_contents = str(args[0]).strip()
    else: # string
        args_contents = args.strip()
    s3.put_object(Bucket=PROJECT_PUBLIC_BUCKET, Key=args_path, Body=args_contents)

    # load additional run files to S3
    s3.put_object(Bucket=PROJECT_PUBLIC_BUCKET, Key=script_path, Body=script)
    s3.put_object(Bucket=PROJECT_PUBLIC_BUCKET, Key=f"{run_path}/receipt.json", Body=json.dumps({
        "name": name,
        "username": username,
        "args": args_url,
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

    # build secret key and (if secrets present) load secrets into database
    secret_key = secrets.token_hex(16) # 32 hexadecimal characters
    if secret_values:
        table = dynamodb.Table(SECRETS_TABLE)
        table.put_item(
            Item={
                "id": secret_key,
                "value": json.dumps(secret_values),
                "ttl": int(time.time()) + SECRET_EXPIRATION_HOURS * 3600
            }
        )

    # initial parameters for job
    kwargs = {
        "jobName": name,
        "jobQueue": f"{STACK_NAME}-{queue}-job-queue",
        "jobDefinition": f"{STACK_NAME}-{image}-job-definition",
        "parameters": {
            "script": script_url,
            "args": args_url,
            "output": run_url
        },
        "containerOverrides": container_overrides,
        "tags": {"user": username, "stack": STACK_NAME}
    }

    # set array processing parameter
    if process_as_array:
        kwargs["arrayProperties"] = {
            "size": len(args)
        }

    # set secret key parameter
    if secret_values:
        kwargs["parameters"]["secret_key"] = secret_key

    # submit job
    response = batch.submit_job(**kwargs)
    print(f'Job <{name}> submitted, aws batch job id = {response["jobId"]}, sliderule runner run id = {run_id}')
    state["job_id"] = response["jobId"]

    # status deployment
    send_email(f"SlideRule Job Submission (@{username} {name})", json.dumps(state, indent=2))

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

    # get optional request variables
    job_state   = body.get("job_state", ["SUBMITTED", "PENDING", "RUNNABLE", "STARTING", "RUNNING", "SUCCEEDED", "FAILED"])
    job_id      = body.get("job_id") # string providing the job id
    queue       = body.get("queue", "default")
    verbose     = body.get("verbose", False)

    # get job states list
    job_states = None
    if isinstance(job_state, list):
        job_states = job_state
    elif isinstance(job_state, str):
        job_states = [job_state]
    else:
        raise RuntimeError(f"Invalid job states supplied: {type(job_state)}")

    # validate job states list
    for state in job_states:
        if not isinstance(state, str):
            raise RuntimeError(f"Invalid job state supplied: {type(state)}")
        elif state not in JOB_STATES:
            raise RuntimeError(f"Unknown job state supplied: {state}")

    # initialize response state
    state = {"report": {js: 0 for js in job_state}}
    if verbose: state["jobs"] = []

    # list jobs
    job_list = list_jobs(job_states, queue, job_id)
    for job in job_list:
        state["report"][job["status"]] += 1
        if verbose:
            state["jobs"].append({"job_id": job["jobId"], "name": job["jobName"], "index": job["arrayProperties"]["index"], "status": job["status"]})

    # success
    return json_response(200, state)

#
# Job Cancel
#
def cancel_handler(body, org_roles):

    # initialize response state
    state = []

    # get optional request variables
    job_list = body.get("job_list")
    queue = body.get("queue", "default")

    # get jobs to delete
    if job_list:
        jobs_to_delete = job_list
    elif "developer" in org_roles:
        jobs_to_delete = [job["jobId"] for job in list_jobs(["SUBMITTED", "PENDING", "RUNNABLE", "STARTING", "RUNNING"], queue, None)]
    else:
        raise RuntimeError("Insufficient permissions to cancel all jobs")

    # delete jobs
    with ThreadPoolExecutor(max_workers=API_CONCURRENCY) as executor:
        futures = {executor.submit(cancel_job, job_id): job_id for job_id in jobs_to_delete}
        for future in as_completed(futures):
            job_id = future.result()
            state.append(job_id)

    # success
    return json_response(200, state)

#
# Job Logs
#
def logs_handler(body):

    # get request parameters
    job_id = body.get("job_id") # string providing the (child) job id

    # get job attributes
    response = batch.describe_jobs(jobs=[job_id])
    if not response["jobs"]:
        raise RuntimeError(f"Job not found: {job_id}")

    # get log stream name
    job = response["jobs"][0]
    log_stream = job["container"].get("logStreamName")
    if not log_stream:
        raise RuntimeError(f"Logs not found: {job_id}")

    # get log events
    events = []
    token = None
    while True:
        kwargs = {
            "logGroupName": f"/aws/batch/{STACK_NAME}",
            "logStreamName": log_stream,
            "startFromHead": True,
        }
        if token is not None:
            kwargs["nextToken"] = token
        response = logs.get_log_events(**kwargs)
        events.extend(event["message"] for event in response["events"])
        next_token = response["nextForwardToken"]
        if next_token == token:
            break  # no progress made -> end of stream
        token = next_token

    # success
    return json_response(200, events)

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

        # get secrets (if provided)
        secret_values = body.pop("secrets", None)

        # log request
        print(f'Received request: {path} {body}')

        # check signature (for all requests)
        if not verify_signature(path, body_raw, username, event):
            return json_response(403, {'error': 'access denied', 'error_description': 'invalid signature'})

        # check organization membership
        if "member" not in org_roles:
            return json_response(403, {'error': 'access denied', 'error_description': 'not a member'})

        # check protected images
        if ("developer" not in org_roles) and ("image" in body) and (body["image"] in PROTECTED_IMAGE_TAGS):
            return json_response(403, {'error': 'access denied', 'error_description': f'insufficient privelege to execute image {body["image"]}'})

        # check for valid queue
        if ("queue" in body) and (body["queue"] not in JOB_QUEUES):
            return json_response(400, {'error': 'invalid request', 'error_description': f'{body["queue"]} not a valid queue'})

        # check job ownership (404 so the existence of other users' jobs isn't revealed)
        if ("job_id" in body) and (get_job_owner(body["job_id"]) != username):
            return json_response(404, {'error': 'not found', 'error_description': 'job not found'})

        # route request
        if path == '/submit': # submits batch runner job
            return submit_handler(body, username, secret_values)
        elif path == '/report/jobs': # returns status report on batch runner jobs
            return report_jobs_handler(body)
        elif path == '/report/queue': # returns report on all the jobs submitted
            return report_queue_handler(body)
        elif path == '/cancel': # cancel a submitted job
            return cancel_handler(body, org_roles)
        elif path == '/logs': # get log events for a job
            return logs_handler(body)

        # invalid path
        return json_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return exception_response(e)
