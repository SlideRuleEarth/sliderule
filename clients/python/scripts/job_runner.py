import os
import sys
import json
import argparse
import boto3
import string
import random
import subprocess

try:

    # ########################
    # Command Line Arguments
    # ########################

    parser = argparse.ArgumentParser(description="""sliderule python job runner""")
    parser.add_argument('script',       type=str) # url of script to execute
    parser.add_argument('arguments',    type=str) # url of argument file
    parser.add_argument('output',       type=str) # url of output directory
    parser.add_argument('secret_key',   type=str) # key to retrieve secrets
    args = parser.parse_args()

    # ########################
    # Globals
    # ########################

    s3 = boto3.client("s3")
    dynamodb = boto3.resource("dynamodb")
    unique = ''.join(random.choices(string.ascii_lowercase, k=7))

    # ########################
    # Helper Functions
    # ########################

    def parse_url(url):
        path = url.split("s3://")[-1]
        bucket = path.split("/")[0]
        key = '/'.join(path.split("/")[1:])
        return bucket, key

    # ########################
    # Get Script
    # ########################

    if args.script.startswith("s3://"):
        local_script = f"/tmp/script-{unique}.py"
        script_bucket, script_key = parse_url(args.script)
        print(f"Downloading {args.script} to {local_script}")
        s3.download_file(script_bucket, script_key, local_script)
    else:
        local_script = args.script

    # ########################
    # Get Arguments
    # ########################

    if args.arguments.startswith("s3://"):
        local_arguments = f"/tmp/args-{unique}.json"
        arguments_bucket, arguments_key = parse_url(args.arguments)
        print(f"Downloading {args.arguments} to {local_arguments}")
        s3.download_file(arguments_bucket, arguments_key, local_arguments)
    else:
        local_arguments = args.arguments

    array_index = os.environ.get("AWS_BATCH_JOB_ARRAY_INDEX")
    if array_index != None:
        with open(local_arguments, "r") as file:
            arguments_array = json.load(file)
            arguments = arguments_array[int(array_index)]
    else:
        with open(local_arguments, "r") as file:
            arguments = file.read()

    # ########################
    # Set Results
    # ########################

    local_result = f"/tmp/result-{unique}.json"

    # ########################
    # Get Secrets
    # ########################

    # retrieve secrets from database
    secrets = {}
    secrets_table = os.environ.get("SECRETS_TABLE")
    if secrets_table and args.secret_key:
        table = dynamodb.Table(secrets_table)
        response = table.get_item(Key={"id": args.secret_key})
        item = response.get("Item")
        if item:
            secrets = json.loads(item["value"])
        else:
            print("Warning: secret was not found")

    # write secrets to file
    local_secrets = f"/tmp/secrets-{unique}.json"
    with open(local_secrets, "w") as file:
        json.dump(secrets, file)

    # ########################
    # Execute Script
    # ########################

    # execute subprocess
    #   e.g. python /tmp/script-<unique>.py args /tmp/result-<unique>.json /tmp/secrets-<unique>.json
    print(f"Running script: {arguments}")
    run_array = [sys.executable, local_script, arguments, local_result, local_secrets]
    result = subprocess.run(run_array, check=False)
    if result.returncode > 0: # uncaught exception
        raise RuntimeError(f"unhandled exception")
    elif result.returncode < 0:
        raise RuntimeError(f"script failed execution: {result.returncode}")

    # ########################
    # Put Result
    # ########################

    if os.path.exists(local_result):
        if args.output.startswith("s3://"):
            output_bucket, output_directory = parse_url(args.output)
            remote_result = f"{output_directory}/result{array_index and str(array_index) or ""}.json"
            if remote_result.startswith("/"):
                remote_result = remote_result[1:]
            print(f"Uploading {local_result} to {remote_result}")
            s3.upload_file(local_result, output_bucket, remote_result)
            os.remove(local_result)
        else:
            os.replace(local_result, args.output)

except Exception as e:

    print(f"Job runner failed: {e}")
    sys.exit(1)
