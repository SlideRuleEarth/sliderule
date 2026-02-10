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
parser.add_argument('--testfile',   type=str,   default=None)
parser.add_argument('--testdir',    type=str,   default="s3://sliderule/cf/testrunner")
parser.add_argument('--branch',     type=str,   default="main")
parser.add_argument('--output',     type=str,   default="/tmp/summary.json")
args,_ = parser.parse_known_args()

# create S3 client
s3_client = boto3.client("s3")

# get bucket and subfolder from url
path = args.testdir.split("s3://")[-1]
bucket = path.split("/")[0]
subfolder = '/'.join(path.split("/")[1:])

if args.testfile != None:
    # hardcode log file
    num_files = 1
    log_files = [args.testfile]
else:
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

# initialize results
results = {"branch": args.branch}
results["latest"] = latest_log_file
results["num_logs"] = len(log_files)
results["total_files"] = num_files
results["date"] = latest_dt.astimezone().strftime("%Y-%m-%d %H:%M:%S")

# download latest log file
local_log_file = f"/tmp/{latest_log_file}"
s3_client.download_file(Bucket=bucket, Key=f"{subfolder}/{latest_log_file}", Filename=local_log_file)

# test parser class
class TestParser:
    def __init__(self, kind, key):
        self.status = False
        self.state = "not found"
        self.lines = []
        self.kind = kind
        self.key = key
    def scrape(self, line):
        if self.state in ['not found']:
            if line == self.key:
                self.state = "searching"
        elif self.state in ['searching', 'in progress']:
            if self.kind == "selftest":
                if self.state == "searching" and line == "*********************************":
                    self.state = "in progress"
                elif self.state == "in progress":
                    if line == "*********************************":
                        self.state = "complete"
                    else:
                        self.lines.append(line)
            elif self.kind == "pytest":
                if self.state == "searching" and line == "=========================== short test summary info ============================":
                    self.state = "in progress"
                elif self.state == "searching" and line.startswith("===========") and ("FAILURES" not in line):
                    self.state = "complete"
                    self.lines.append(line)
                elif self.state == "in progress":
                    if line.startswith("==========="):
                        self.state = "complete"
                    self.lines.append(line)
    def parse(self):
        result = {}
        if self.kind == "selftest":
            result["state"] = self.state
            for line in self.lines:
                if "number of tests" in line:
                    result["tests"] = int(line.split(":")[1].strip())
                elif "number of skipped tests" in line:
                    result["skipped"] = int(line.split(":")[1].strip())
                elif "number of asserts" in line:
                    result["asserts"] = int(line.split(":")[1].strip())
                elif "number of errors" in line:
                    result["errors"] = int(line.split(":")[1].strip())
        elif self.kind == "pytest":
            result["state"] = self.state
            if self.state != "complete":
                return
            result["errors"] = 0
            result["summary"] = []
            for line in self.lines:
                if line.startswith("FAILED") or line.startswith("ERROR"):
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
        self.status = result.get("errors") == 0
        return result

# open up latest log file and search for results
selftest = TestParser("selftest", "/sliderule/stage/sliderule/bin/sliderule /sliderule/targets/slideruleearth/test_runner.lua")
sliderule = TestParser("pytest", "rootdir: /sliderule/clients/python")
provisioner = TestParser("pytest", "rootdir: /sliderule/applications/provisioner")
ams = TestParser("pytest", "rootdir: /sliderule/applications/ams")
with open(local_log_file, "r") as file:
    for line in file.readlines():
        line = line.strip()
        selftest.scrape(line)
        sliderule.scrape(line)
        provisioner.scrape(line)
        ams.scrape(line)

# parse results
results["selftest"] = selftest.parse()
results["sliderule"] = sliderule.parse()
results["provisioner"] = provisioner.parse()
results["ams"] = ams.parse()

# determine pass/fail
results["status"] = selftest.status and sliderule.status and provisioner.status and ams.status

# output results
print(json.dumps(results, indent=2))
with open(args.output, "w") as file:
    file.write(json.dumps(results))