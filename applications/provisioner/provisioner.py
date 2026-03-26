import os
import re
import json
import base64
import socket
import boto3
import botocore.exceptions
from string import Template
from datetime import datetime, timedelta, timezone
from cryptography.hazmat.primitives.serialization import load_ssh_public_key
import scheduler

# ###############################
# Cached Objects
# ###############################

ec2 = boto3.client("ec2")
s3 = boto3.client("s3")
cf = boto3.client("cloudformation")
ev = boto3.client('events')
ses = boto3.client('ses')
sm = boto3.client('secretsmanager')

_pubkey_cache = {}

# ###############################
# Globals
# ###############################

STACK_NAME = os.environ.get("STACK_NAME")
DOMAIN = os.environ.get("DOMAIN")
PUBLIC_CLUSTER = os.environ.get('PUBLIC_CLUSTER')
PROJECT_BUCKET = os.environ.get("PROJECT_BUCKET")
PROJECT_FOLDER = os.environ.get("PROJECT_FOLDER")
PROJECT_PUBLIC_BUCKET = os.environ.get("PROJECT_PUBLIC_BUCKET")
CONTAINER_REGISTRY = os.environ.get('CONTAINER_REGISTRY')
JWT_ISSUER = os.environ.get('JWT_ISSUER')
ALERT_STREAM = os.environ.get('ALERT_STREAM')
TELEMETRY_STREAM = os.environ.get('TELEMETRY_STREAM')
ENVIRONMENT_VERSION = os.environ.get('ENVIRONMENT_VERSION')
AFFILIATES_FILENAME = os.environ.get('AFFILIATES_FILENAME')
SUPPORT_EMAIL = os.environ.get('SUPPORT_EMAIL')
ALERT_EMAIL = os.environ.get('ALERT_EMAIL')

SYSTEM_KEYWORDS = ['login','provisioner','client','recorder','runner','mcp','sliderule','monitor']

# ###############################
# Utilities
# ###############################

#
# Checks on cluster name
#
def valid_cluster_name(cluster):
    if (not isinstance(cluster, str)) or \
       (cluster in SYSTEM_KEYWORDS) or \
       (not re.match(r'^[A-Za-z0-9-_]+$', cluster)) or \
       (len(cluster) > 40):
        return False
    else:
        return True

#
# Convention for deriving stack name from cluster
#
def build_cluster_stack_name(cluster):
    if not isinstance(cluster, str):
        return None
    elif valid_cluster_name(cluster):
        return f'{cluster}-cluster'
    else:
        raise RuntimeError(f"Invalid cluster name: {cluster}")

#
# Convention for deriving user asg stack name from cluster stack name
#
def build_user_asg_stack_name(cluster_stack_name, username):
    if not isinstance(cluster_stack_name, str):
        return None
    elif valid_cluster_name(cluster_stack_name):
        return f'{cluster_stack_name}-{username}-asg'
    else:
        raise RuntimeError(f"Invalid cluster stack name: {cluster_stack_name}")

#
# Get tags for EC2 instances
#
def get_instances_by_name(name):
    try:
        response = ec2.describe_instances(
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
# Get full EC2 instance name by name filter and IP address
#
def get_instance_by_ip(name_filter, ip):
    try:
        response = ec2.describe_instances(
            Filters=[
                { "Name": "tag:Name",   "Values": [name_filter] },
                { "Name": "ip-address", "Values": [ip] }
            ]
        )
        instances = [instance for reservation in response["Reservations"] for instance in reservation["Instances"]]
        if len(instances) > 1:
            raise RuntimeError(f"Multiple ({len(instances)}) instances matched")
        return next((t["Value"] for t in instances[0]["Tags"] if t["Key"] == "Name"), None)
    except Exception as e:
        print(f"Unable to get <{name_filter}> instance with IP <{ip}>: {e}")
        return None

#
# Get stack description
#
def get_stack_description(stack_name):
    rsp = cf.describe_stacks(StackName=stack_name)
    return rsp["Stacks"][0]

#
# Get stack outputs
#
def get_stack_outputs(stack_name):
    stack = get_stack_description(stack_name)
    outputs = stack.get("Outputs", [])
    return {output["OutputKey"]:output["OutputValue"] for output in outputs}

#
# Clean description
#
def clean_description(item):
    description = {}
    for k, v in item.items():
        if hasattr(v, "tolist"):
            description[k] = v.tolist()
        elif isinstance(v, (datetime)):
            description[k] = v.isoformat()
        elif not isinstance(v, (bytes, bytearray)):
            description[k] = v
    return description

#
# Get resources from a stack
#
def get_stack_resources(stack_name):
    resources = cf.describe_stack_resources(StackName=stack_name)['StackResources']
    return {r['LogicalResourceId']: r['PhysicalResourceId'] for r in resources}

#
# Get user asg stacks for parent cluster stack
#
def get_cluster_stack_names(cluster_stack_name):
    try:
        paginator = cf.get_paginator('describe_stacks')
        return [
            stack['StackName'] for page in paginator.paginate()
            for stack in page['Stacks']
            if stack['StackName'].startswith(f"{cluster_stack_name}")
            and stack['StackStatus'] not in ('DELETE_COMPLETE', 'DELETE_IN_PROGRESS')
        ]
    except Exception as e:
        print(f"Failed to get user asg stacks: {e}")
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
        return json_response(404, {'error': 'does not exist', 'error_description': f'{e}'})
    return json_response(500, {'error': 'internal error', 'error_description': f'{e}'})

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
# Get User Affiliation
#
def get_affiliation(username):
    """
    Retrieve and return dictionary of affiliation attributes for provided user
    """
    try:
        response = s3.get_object(Bucket=PROJECT_BUCKET, Key=f"{PROJECT_FOLDER}/{AFFILIATES_FILENAME}")
        data = json.load(response['Body']).get(username, {})
        return data.get("active", False) and data or None
    except Exception as e:
        print(f"Failed to get the affiliates file: {e}")
        return None

#
# Populate User Data
#
def populate_user_data(script, rqst, service):
    """
    Populates environment variables of a user data script using the environment and request parameters
    """
    return Template(open(script).read()).safe_substitute(os.environ | {
        "VERSION": rqst["version"],
        "IS_PUBLIC": json.dumps(rqst["is_public"]),
        "CLUSTER": rqst["cluster"],
        "SERVICE": service
    })

# ###############################
# Business Logic
# ###############################

#
# Verify Public/Private Key Signature
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
# Max Nodes
#
def get_max_nodes(org_roles):
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
    audiences = parse_claim_array(claims.get('aud', "[]"))
    deployable_clusters = {audience for audience in audiences if (valid_cluster_name(audience) or audience == '*')}
    known_clusters = deployable_clusters - {'*'} | {'sliderule'}
    max_nodes = get_max_nodes(org_roles)
    max_ttl = get_max_ttl(org_roles)
    if 'affiliate' in org_roles:
        affiliation = get_affiliation(username) or {}
        max_nodes = affiliation.get('max_nodes', max_nodes)
        max_ttl = affiliation.get('max_ttl', max_nodes)
    return {
        'username': username,
        'orgRoles': org_roles,
        'knownClusters': list(known_clusters),
        'deployableClusters': list(deployable_clusters),
        'maxNodes': max_nodes,
        'maxTTL': max_ttl
    }

#
# Validate Request
#
def validate_request(event, info):

    # configuration
    MIN_TTL_FOR_AUTOSHUTDOWN = 15 # minutes

    # pull out required request parameters
    path = event.get("rawPath", '')
    body_raw = event.get("body", '')
    if event.get("isBase64Encoded"):
        body_raw = base64.b64decode(body_raw).decode("utf-8")
    if body_raw:
        body = json.loads(body_raw)
    else:
        body = {}
    print(f'Received request: {path} {body}') # diagnostic

    # check signature (for owners only)
    if ("owner" in info["orgRoles"]) or ('*' in info["deployableClusters"]):
        if not verify_signature(path, body_raw, info["username"], event):
            return None

    # check organization membership
    if ('member' not in info["orgRoles"]) and ('affiliate' not in info["orgRoles"]):
        print(f'Access denied to {info["username"]}, organization roles: {info["orgRoles"]}')
        return None

    # get public cluster (only for user services)
    public_cluster_name = None
    path_components = path.split("/")
    if (len(path_components) >= 3) and (path_components[-1] == info["username"]):
        public_ip = socket.gethostbyname(f"{PUBLIC_CLUSTER}.{DOMAIN}")
        public_cluster_ilb_name = get_instance_by_ip(f"{PUBLIC_CLUSTER}-*-ilb", public_ip)
        if public_cluster_ilb_name:
            public_cluster_name = public_cluster_ilb_name.split("-ilb")[0]
        else:
            print(f'Unable to locate public cluster for user service request to {PUBLIC_CLUSTER}.{DOMAIN}')

    # check cluster
    cluster = body.get("cluster")
    if cluster:
        if cluster == PUBLIC_CLUSTER:
            cluster = public_cluster_name # only set if user service requested
        elif (cluster not in info["deployableClusters"]) and ('*' not in info["deployableClusters"]):
            print(f'Access denied to {info["username"]}, {cluster} not in allowed clusters: {info["deployableClusters"]}')
            return None
        if not valid_cluster_name(cluster):
            print(f'Access denied to {info["username"]}, invalid cluster: {cluster}')
            return None

    # check node_capacity
    node_capacity = body.get("node_capacity")
    if node_capacity and ((int(node_capacity) > info["maxNodes"]) or (int(node_capacity) < 1)):
        print(f'Access denied to {info["username"]}, node capacity: {node_capacity}, max nodes: {info["maxNodes"]}')
        return None

    # check ttl
    ttl = body.get("ttl")
    if ttl and ((int(ttl) > info["maxTTL"]) or (int(ttl) < MIN_TTL_FOR_AUTOSHUTDOWN)):
        print(f'Access denied to {info["username"]}, invalid ttl: {ttl}')
        return None

    # build and return request info
    return {
        "path": path,
        "username": info["username"],
        "member": 'member' in info["orgRoles"],
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
def deploy_handler(rqst, kind):

    # initialize response state
    state = {}

    # get arns for auto-shutdown
    outputs = get_stack_outputs(STACK_NAME)
    destroy_lambda_arn = outputs["DestroyLambdaArn"]
    schedule_lambda_arn = outputs["ScheduleLambdaArn"]

    # get cluster stack name following the naming convention
    cluster_stack_name = build_cluster_stack_name(rqst["cluster"])

    # handle cluster deployments
    if kind == 'cluster':

        # get user data for instances
        ilb_user_data = populate_user_data("ilb.sh", rqst, rqst["cluster"])
        monitor_user_data = populate_user_data("monitor.sh", rqst, rqst["cluster"])
        node_user_data = populate_user_data("node.sh", rqst, rqst["cluster"])

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Version", "ParameterValue": rqst["version"]},
            {"ParameterKey": "IsPublic", "ParameterValue": json.dumps(rqst["is_public"])},
            {"ParameterKey": "Domain", "ParameterValue": DOMAIN},
            {"ParameterKey": "Cluster", "ParameterValue": rqst["cluster"]},
            {"ParameterKey": "ProjectBucket", "ParameterValue": PROJECT_BUCKET},
            {"ParameterKey": "ProjectPublicBucket", "ParameterValue": PROJECT_PUBLIC_BUCKET},
            {"ParameterKey": "DestroyLambdaArn", "ParameterValue": destroy_lambda_arn},
            {"ParameterKey": "ScheduleLambdaArn", "ParameterValue": schedule_lambda_arn},
            {"ParameterKey": "NodeCapacity", "ParameterValue": str(rqst["node_capacity"])},
            {"ParameterKey": "TTL", "ParameterValue": str(rqst["ttl"])},
            {"ParameterKey": "AlertStream", "ParameterValue": ALERT_STREAM},
            {"ParameterKey": "TelemetryStream", "ParameterValue": TELEMETRY_STREAM},
            {"ParameterKey": "IlbUserData", "ParameterValue": ilb_user_data},
            {"ParameterKey": "MonitorUserData", "ParameterValue": monitor_user_data},
            {"ParameterKey": "NodeUserData", "ParameterValue": node_user_data}
        ]

        # read template
        templateBody = open("cluster.yml").read()

        # create stack
        state["response"] = cf.create_stack(StackName=cluster_stack_name, TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

        # status deployment
        send_email(f"SlideRule Cluster Deployed ({rqst['cluster']}/{rqst['username']}/{rqst['node_capacity']}/{rqst['ttl']})", json.dumps(state, indent=2))
        print(f'Deploy initiated for {cluster_stack_name}')

    # handle user asg deployments
    elif kind == 'user':

        # get user data for instances
        ilb_user_data = populate_user_data("ilb.sh", rqst, rqst["username"])
        monitor_user_data = populate_user_data("monitor.sh", rqst, rqst["username"])
        node_user_data = populate_user_data("node.sh", rqst, rqst["username"])

        # get parent stack resources
        resources = get_stack_resources(cluster_stack_name)
        cluster_subnet = resources["ClusterSubnet"]
        cluster_node_sg = resources["ClusterNodeSG"]
        instance_profile = resources["InstanceProfile"]

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Cluster", "ParameterValue": rqst["cluster"]},
            {"ParameterKey": "Username", "ParameterValue": rqst["username"]},
            {"ParameterKey": "NodeCapacity", "ParameterValue": str(rqst["node_capacity"])},
            {"ParameterKey": "TTL", "ParameterValue": str(rqst["ttl"])},
            {"ParameterKey": "ClusterSubnet", "ParameterValue": cluster_subnet},
            {"ParameterKey": "ClusterNodeSG", "ParameterValue": cluster_node_sg},
            {"ParameterKey": "InstanceProfile", "ParameterValue": instance_profile},
            {"ParameterKey": "DestroyLambdaArn", "ParameterValue": destroy_lambda_arn},
            {"ParameterKey": "ScheduleLambdaArn", "ParameterValue": schedule_lambda_arn},
            {"ParameterKey": "NodeUserData", "ParameterValue": node_user_data}
        ]

        # read template
        templateBody = open("userasg.yml").read()

        # the stack name naming convention for user asg
        user_asg_stack_name = build_user_asg_stack_name(cluster_stack_name, rqst["username"])

        # create stack
        state["response"] = cf.create_stack(StackName=user_asg_stack_name, TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

        # status deployment
        send_email(f"SlideRule Cluster Expanded ({rqst['cluster']}/{rqst['username']}/{rqst['node_capacity']}/{rqst['ttl']})", json.dumps(state, indent=2))
        print(f'Expand initiated for {user_asg_stack_name}')

    # return success
    return json_response(200, state)

#
# Extend
#
def extend_handler(rqst, kind):

    # initialize response state
    state = {}

    # get rule name
    cluster_stack_name = build_cluster_stack_name(rqst["cluster"])
    if kind == 'cluster':
        rule_name = f'{cluster_stack_name}-auto-shutdown'
    elif kind == 'user':
        rule_name = f'{build_user_asg_stack_name(cluster_stack_name, rqst["username"])}-auto-shutdown'

    # calculate new shutdown time
    new_shutdown_time = datetime.now(timezone.utc) + timedelta(minutes=rqst["ttl"])
    state["cron_expression"] = f'cron({new_shutdown_time.minute} {new_shutdown_time.hour} {new_shutdown_time.day} {new_shutdown_time.month} ? {new_shutdown_time.year})'

    # extend rule
    state["response"] = ev.put_rule(
        Name=rule_name,
        ScheduleExpression=state["cron_expression"],
        State='ENABLED',
        Description=f'Automatic shutdown for {rule_name} (updated)'
    )

    # return success
    return json_response(200, state)

#
# Destroy
#
def lambda_destroy(event, context):

    try:
        # initialize response status
        state = {"status": True}

        # get stack name to delete
        cluster = event.get("cluster") # schedule deletions do not supply cluster parameter
        parent_stack_name = event.get("stack_name", build_cluster_stack_name(cluster)) # scheduled deletions pass stack name

        # get list of stacks to delete
        stack_names = get_cluster_stack_names(parent_stack_name)

        # delete all stacks
        for stack_name in sorted(stack_names, reverse=True):

            # initialize state info for stack
            state[stack_name] = {}
            info = state[stack_name]

            # delete eventbridge target and rule
            rule_name = f'{stack_name}-auto-shutdown'
            print(f'Delete initiated for {rule_name}')
            try:
                ev.remove_targets(Rule=rule_name, Ids=["1"])
                info["EventBridge Target Removed"] = True
            except Exception as e:
                print(f'Unable to delete eventbridge rule: {e}')
                info["EventBridge Target Removed"] = False
            try:
                ev.delete_rule(Name=rule_name, Force=False)
                info["EventBridge Rule Removed"] = True
            except Exception as e:
                print(f'Unable to delete eventbridge rule: {e}')
                info["EventBridge Rule Removed"] = False

            # delete stack
            info["response"] = cf.delete_stack(StackName=stack_name)
            print(f'Delete initiated for {stack_name}')

    # explicipty handle exceptions
    except Exception as e:
        print(f'Exception in destroy: {e}')
        state["exception"] = f'Failure in destroy'
        state["status"] = False

    # return response directly
    return state

#
# Status
#
def status_handler(rqst, kind):

    # initialize response state
    state = {}

    # get parent cluster stack name
    cluster_stack_name = build_cluster_stack_name(rqst["cluster"])

    # status cluster
    if kind == 'cluster':

        # status stack
        cluster_stack = get_stack_description(cluster_stack_name)
        state["response"] = clean_description(cluster_stack)

        # attempt to get auto-shutdown time
        rule_name = scheduler.build_rule_name(cluster_stack_name)
        state["auto_shutdown"] = scheduler.get_next_trigger_time(rule_name)

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

    # status user asg
    elif kind == 'user':

        # stack status
        user_asg_stack_name = build_user_asg_stack_name(cluster_stack_name, rqst["username"])
        user_asg_stack = get_stack_description(user_asg_stack_name)
        state["response"] = clean_description(user_asg_stack)

        # attempt to get auto-shutdown time
        user_asg_rule_name = scheduler.build_rule_name(user_asg_stack_name)
        state["auto_shutdown"] = scheduler.get_next_trigger_time(user_asg_rule_name)

        # get number of nodes
        user_asg_node_instances = get_instances_by_name(f'{rqst["cluster"]}-node-{rqst["username"]}')
        state["current_nodes"] = len(user_asg_node_instances)

    # return success
    return json_response(200, state)

#
# Events
#
def events_handler(rqst, kind):

    # get events for stack
    cluster_stack_name = build_cluster_stack_name(rqst["cluster"])

    # get stack name
    if kind == 'cluster':
        stack_name = cluster_stack_name
    elif kind == 'user':
        stack_name = build_user_asg_stack_name(cluster_stack_name, rqst["username"])

    # get stack events
    description = cf.describe_stack_events(StackName=stack_name)
    stack_events = description["StackEvents"]
    print(f'Events requested for {stack_name}')

    # build cleaned response
    response = [clean_description(stack_event) for stack_event in stack_events]

    # return success
    return json_response(200, response)

#
# Cluster Report
#
def report_clusters_handler(cluster):

    # initialize report
    report = {}

    # get list of intelligent load balancers with their tags
    for instance in get_instances_by_name('*-ilb'):
        tags = {t['Key']: t['Value'] for t in instance.get('Tags', [])}
        name = tags.get('Name')
        if name:
            # get status of each cluster containing the intelligent load balancer
            cluster_name = name.split("-ilb")[0]
            if cluster and cluster_name != cluster:
                continue # for reports that are only for one cluster
            details = status_handler({"cluster": cluster_name}, 'cluster')
            if details["statusCode"] == 200:
                status = json.loads(details["body"])
                report[cluster_name] = { k: status.get(k) for k in ["auto_shutdown", "current_nodes", "version", "is_public", "node_capacity"] }
                # get asgs for cluster and status them
                report[cluster_name]["users"] = {}
                asg_stack_names = get_cluster_stack_names(build_cluster_stack_name(cluster_name))
                for asg_stack_name in asg_stack_names:
                    stack_name_components = asg_stack_name.split("-")
                    kind = stack_name_components[-1]
                    if kind == 'asg':
                        username = stack_name_components[-2]
                        details = status_handler({"cluster": cluster_name, "username": username}, 'user')
                        if details["statusCode"] == 200:
                            status = json.loads(details["body"])
                            report[cluster_name]["users"][username] = { k: status.get(k) for k in ["auto_shutdown", "current_nodes"] }

    # return success
    return json_response(200, report)

#
# Test Report
#
def report_tests_handler(rqst):

    # get test summary
    summary_file = f"{rqst["branch"]}-summary.json"
    s3.download_file(Bucket=PROJECT_BUCKET, Key=f"{PROJECT_FOLDER}/testrunner/{summary_file}", Filename=f"/tmp/{summary_file}")

    # read test summary
    with open(f"/tmp/{summary_file}", "r") as file:
        report = json.loads(file.read())

    # return success
    return json_response(200, report)

#
# Test Runner
#
def deploy_test_handler(rqst):

    try:
        # initialize response status
        state = {}

        # get arns for auto-shutdown
        resp = cf.describe_stacks(StackName=STACK_NAME)
        outputs = resp["Stacks"][0].get("Outputs", [])
        destroy_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "DestroyLambdaArn")
        scheduler_lambda_arn = next(output["OutputValue"] for output in outputs if output["OutputKey"] == "ScheduleLambdaArn")

        # build parameters for stack creation
        state["parms"] = [
            {"ParameterKey": "Domain", "ParameterValue": DOMAIN},
            {"ParameterKey": "EnvironmentVersion", "ParameterValue": ENVIRONMENT_VERSION},
            {"ParameterKey": "ProjectBucket", "ParameterValue": PROJECT_BUCKET},
            {"ParameterKey": "ProjectFolder", "ParameterValue": PROJECT_FOLDER},
            {"ParameterKey": "ContainerRegistry", "ParameterValue": CONTAINER_REGISTRY},
            {"ParameterKey": "DestroyLambdaArn", "ParameterValue": destroy_lambda_arn},
            {"ParameterKey": "ScheduleLambdaArn", "ParameterValue": scheduler_lambda_arn},
            {"ParameterKey": "DeployDate", "ParameterValue": datetime.now().strftime("%Y%m%d%H%M%S")},
            {"ParameterKey": "Branch", "ParameterValue": rqst["branch"]}
        ]

        # read template
        templateBody = open("testrunner.yml").read()

        # create stack (default to hardcoded stack name so only one can run at a time)
        state["response"] = cf.create_stack(StackName='testrunner', TemplateBody=templateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=state["parms"])

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
        info = get_user_info(claims) # pull out user information from claims
        rqst = validate_request(event, info) # validate request against claims and return safe request parameters

        # route request
        if rqst == None: # check request
            return json_response(403, {'error': 'access denied'})

        elif rqst["path"] == '/info': # only uses info, but still requires validated rqst
            return json_response(200, info)

        elif rqst["path"] == '/deploy': # deploys a cluster
            return deploy_handler(rqst, 'cluster')

        elif rqst["path"] == f'/deploy/{rqst["username"]}': # deploys a user asg connected to a cluster
            return deploy_handler(rqst, 'user')

        elif rqst["path"] == '/extend': # extends the TTL of a cluster
            return extend_handler(rqst, 'cluster')

        elif rqst["path"] == f'/extend/{rqst["username"]}': # extends the TTL of a user asg connected to a cluster
            return extend_handler(rqst, 'user')

        elif rqst["path"] == '/destroy': # destroys a cluster
            return lambda_destroy(rqst, None)

        elif rqst["path"] == f'/destroy/{rqst["username"]}': # destroys a user asg connected to a cluster
            return lambda_destroy(rqst | {"stack_name": build_user_asg_stack_name(build_cluster_stack_name(rqst["cluster"]), rqst["username"])}, None)

        elif rqst["path"] == '/status': # returns deployment status of a cluster
            return status_handler(rqst, 'cluster')

        elif rqst["path"] == f'/status/{rqst["username"]}': # returns deployment status of a user asg connected to a cluster
            return status_handler(rqst, 'user')

        elif rqst["path"] == '/events': # returns cloudformation stack events for a cluster deployment
            return events_handler(rqst, 'cluster')

        elif rqst["path"] == f'/events/{rqst["username"]}': # returns cloudformation stack events for a user asg connected to a cluster deployment
            return events_handler(rqst, 'user')

        elif rqst["member"]: # member only APIs

            if rqst["path"] == '/report/clusters': # returns report of clusters that have been deployed
                return report_clusters_handler(None)

            elif rqst["path"] == f'/report/clusters/{rqst["cluster"]}': # returns report of clusters that have been deployed
                return report_clusters_handler(rqst["cluster"])

            elif rqst["path"] == '/report/tests': # returns report on status of last test runner deployment
                return report_tests_handler(rqst)

            elif rqst["path"] == '/test': # deploys test runner
                return deploy_test_handler(rqst)

        # invalid path
        return json_response(404, {'error': 'not found'})

    except Exception as e:

        # unhandled exception
        return exception_reponse(e)

# ###############################
# Main: Local Test Environment
# ###############################

if __name__ == '__main__':

    # imports
    import sliderule
    import argparse

    # command line arguments
    parser = argparse.ArgumentParser(description="""Provisioner Command Line""")
    parser.add_argument('--cluster',        type=str,               default=os.environ.get("CLUSTER"))
    parser.add_argument('--node_capacity',  type=int,               default=os.environ.get("NODE_CAPACITY"))
    parser.add_argument('--ttl',            type=int,               default=os.environ.get("TTL"))
    parser.add_argument('--is_public',      action='store_true',    default=(os.environ.get("IS_PUBLIC") == "true"))
    parser.add_argument('--version',        type=str,               default=os.environ.get("VERSION"))
    parser.add_argument('--branch',         type=str,               default=os.environ.get("BRANCH"))
    parser.add_argument('--api',            type=str,               default=None)
    parser.add_argument('--user_service',   action='store_true',    default=False)
    parser.add_argument('--verbose',        action='store_true',    default=False)
    args,_ = parser.parse_known_args()

    # sliderule python client session
    session = sliderule.create_session(domain=DOMAIN, cluster=args.cluster, verbose=args.verbose, user_service=args.user_service)

    # request parameters
    path = args.user_service and f"{args.api}/{session.ps_metadata["sub"]}" or args.api
    body = json.dumps({
        "cluster": args.cluster,
        "node_capacity": str(args.node_capacity),
        "ttl": str(args.ttl),
        "is_public": args.is_public,
        "version": args.version,
        "branch": args.branch
    })

    # sign request
    headers = {}
    session._Session__signrequest(headers, f"{DOMAIN}{path}", body)

    # build request
    rqst = {
        "requestContext": {
            "authorizer": {
                "jwt": {
                    "claims": {
                        "org_roles": f'[{" ".join(session.ps_metadata["org_roles"])}]',
                        "aud": f'[{" ".join(session.ps_metadata["aud"])}]',
                        "sub": f'{session.ps_metadata["sub"]}'
                    }
                }
            }
        },
        "headers": {
            "host": DOMAIN,
            "x-sliderule-timestamp": headers["x-sliderule-timestamp"],
            "x-sliderule-signature": headers["x-sliderule-signature"],
        },
        "rawPath": path,
        "body": body
    }

    # make request
    rsps = lambda_gateway(rqst, None)

    # display response
    if rsps.get("statusCode") == 200:
        content = json.loads(rsps["body"])
        print(json.dumps(content, indent=2))
    else:
        print(json.dumps(rsps, indent=2))