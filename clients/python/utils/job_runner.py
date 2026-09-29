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
    parser.add_argument('arguments',    type=str) # url of argument file OR argument string
    parser.add_argument('output',       type=str) # url of output directory
    args = parser.parse_args()

    # ########################
    # Globals
    # ########################

    s3 = boto3.client("s3")
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
    if array_index:
        with open(local_arguments, "r") as file:
            arguments_array = json.load(file)
            arguments = arguments_array[int(array_index)]
    else:
        with open(local_arguments, "r") as file:
            arguments = file.read()

    # ########################
    # Execute Script
    # ########################

    # build array of arguments to pass to subprocess
    run_array = [sys.executable, local_script]
    for argument in arguments.split(' '):
        run_array.append(argument)

    # add local result file to subprocess arguments
    local_result = f"/tmp/result-{unique}.json"
    run_array.append(local_result)

    # execute subprocess
    print(f"Running script: {arguments}")
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
