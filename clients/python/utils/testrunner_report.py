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
import sys
import boto3

# command line arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--testdir',            type=str,               default="s3://sliderule/cf/testrunner")
parser.add_argument('--branch',             type=str,               default="main")
parser.add_argument('--exclude_empty',      action='store_true',    default=False)
args,_ = parser.parse_known_args()

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
    # display status
    print("#", end='')
    sys.stdout.flush()
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

# display results
print(f'\nTest Runner\n===========')
print(f'Total Log Files:                        {num_files}')
print(f'Log Files for <{args.branch}>:          {len(log_files)}')
print(f'Latest Log File for <{args.branch}>:    {latest_log_file}')
print(f'Date of Latest for <{args.branch}>:     {latest_dt.astimezone().strftime("%Y-%m-%d %H:%M:%S")}')
