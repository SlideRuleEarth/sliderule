# Copyright (c) 2026, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import argparse
from datetime import datetime, timezone
import json
import boto3

# command line arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--testdir',    type=str,   default="s3://sliderule/cf/testrunner")
parser.add_argument('--branch',     type=str,   default="main")
parser.add_argument('--output',     type=str,   default="/tmp/summary.json")
args,_ = parser.parse_known_args()

# initialize results
results = {"branch": args.branch, "selftest": {}, "pytest": {}, "provisioner": {}, "ams": {}}

# create S3 client
s3_client = boto3.client("s3")

# get bucket and subfolder from url
path = args.testdir.split("s3://")[-1]
bucket = path.split("/")[0]
subfolder = '/'.join(path.split("/")[1:])

# get list of reports
num_files = 0
log_files = []
is_truncated = True
continuation_token = None
while is_truncated:
    # make request
    if continuation_token:
        response = s3_client.list_objects_v2(Bucket=bucket, Prefix=subfolder, ContinuationToken=continuation_token)
    else:
        response = s3_client.list_objects_v2(Bucket=bucket, Prefix=subfolder)
    # parse contents
    if 'Contents' in response:
        for obj in response['Contents']:
            num_files += 1
            filename = obj['Key'].split("/")[-1]
            if filename.startswith(f"{args.branch}-") and filename.endswith("-testrunner.log"):
                log_files.append(filename)
    # check if more data is available
    is_truncated = response['IsTruncated']
    continuation_token = response.get('NextContinuationToken')

# find latest log file
latest_log_file = None
latest_dt = datetime(2000,1,1,12,0,0).replace(tzinfo=timezone.utc) # initial minimum timestamp
for log_file in log_files:
    timestamp = log_file.split("-")[1]
    dt = datetime.strptime(timestamp, "%Y%m%d%H%M%S").replace(tzinfo=timezone.utc)
    if dt > latest_dt:
        latest_log_file = log_file
        latest_dt = dt

# populate log results
results["latest"] = latest_log_file
results["num_logs"] = len(log_files)
results["total_files"] = num_files
results["date"] = latest_dt.astimezone().strftime("%Y-%m-%d %H:%M:%S")

# download latest log file
local_log_file = f"/tmp/{latest_log_file}"
s3_client.download_file(Bucket=bucket, Key=f"{subfolder}/{latest_log_file}", Filename=local_log_file)

# open up latest log file and search for results
selftest_state = "start"
selftest_lines = []
sliderule_pytest_state = "start"
sliderule_pytest_lines = []
provisioner_pytest_state = "start"
provisioner_pytest_lines = []
ams_pytest_state = "start"
ams_pytest_lines = []
pytest_state = "None"
with open(local_log_file, "r") as file:
    for line in file.readlines():
        line = line.strip()

        # selftest
        if selftest_state == "start" and line == "*********************************":
            selftest_state = "in progress"
        elif selftest_state == "in progress":
            if line == "*********************************":
                selftest_found = "end"
            else:
                selftest_lines.append(line)

        # pytest state
        if line == "rootdir: /sliderule/clients/python":
            pytest_state = "sliderule"
        elif line == "rootdir: /sliderule/applications/provisioner":
            pytest_state = "provisioner"
        elif line == "rootdir: /sliderule/applications/ams":
            pytest_state = "ams"

        # sliderule - pytest
        if pytest_state == "sliderule":
            if sliderule_pytest_state == "start" and line == "=========================== short test summary info ============================":
                sliderule_pytest_state = "in progress"
            elif sliderule_pytest_state == "in progress":
                if line.startswith("=================="):
                    sliderule_pytest_state = "end"
                sliderule_pytest_lines.append(line)

        # provisioner - pytest
        if pytest_state == "provisioner":
            if provisioner_pytest_state == "start" and line == "=========================== short test summary info ============================":
                provisioner_pytest_state = "in progress"
            elif provisioner_pytest_state == "in progress":
                if line.startswith("=================="):
                    provisioner_pytest_state = "end"
                provisioner_pytest_lines.append(line)

        # ams - pytest
        if pytest_state == "ams":
            if ams_pytest_state == "start" and line == "=========================== short test summary info ============================":
                ams_pytest_state = "in progress"
            elif ams_pytest_state == "in progress":
                if line.startswith("=================="):
                    ams_pytest_state = "end"
                ams_pytest_lines.append(line)

# parse selftest output
for line in selftest_lines:
    if "number of tests" in line:
        results["selftest"]["tests"] = int(line.split(":")[1].strip())
    elif "number of skipped tests" in line:
        results["selftest"]["skipped"] = int(line.split(":")[1].strip())
    elif "number of asserts" in line:
        results["selftest"]["asserts"] = int(line.split(":")[1].strip())
    elif "number of errors" in line:
        results["selftest"]["errors"] = int(line.split(":")[1].strip())

# local helper function to parse pytest output
def parse_pytest(result, lines):
    print("LINES", lines)
    result["errors"] = 0
    result["summary"] = []
    for line in lines:
        if line.startswith("FAILED"):
            result["errors"] += 1
            result["summary"].append(line.split(" ")[1].strip())
        elif line.startswith("=================="):
            toks = line.split(" ")
            for i in range(1, len(toks)):
                if "failed" in toks[i]:
                    result["failed"] = int(toks[i-1].split(" ")[0].strip())
                if "skipped" in toks[i]:
                    result["skipped"] = int(toks[i-1].split(" ")[0].strip())
                if "passed" in toks[i]:
                    result["passed"] = int(toks[i-1].split(" ")[0].strip())

# parse pytest output
parse_pytest(results["pytest"], sliderule_pytest_lines)
parse_pytest(results["provisioner"], provisioner_pytest_lines)
parse_pytest(results["ams"], ams_pytest_lines)

# output results
print(json.dumps(results, indent=2))
with open(args.output, "w") as file:
    file.write(json.dumps(results))