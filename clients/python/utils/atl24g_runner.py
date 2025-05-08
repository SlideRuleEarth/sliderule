# Copyright (c) 2021, University of Washington
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

from sliderule import sliderule, icesat2
import argparse
import time
import threading
import queue
import logging
import sys
import glob
import boto3

# Command Line Arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--domain',             type=str,               default="slideruleearth.io")
parser.add_argument('--organization',       type=str,               default="bathy")
parser.add_argument('--desired_nodes',      type=int,               default=None)
parser.add_argument('--time_to_live',       type=int,               default=600) # 10 hours
parser.add_argument('--log_file',           type=str,               default="atl24.log")
parser.add_argument('--concurrency',        type=int,               default=50)
parser.add_argument('--startup_separation', type=int,               default=2) # seconds
parser.add_argument('--exclude_empty',      action='store_true',    default=False)
parser.add_argument('--input_file',         type=str,               default="testsets/bluetopo_granules.txt")
parser.add_argument('--input_files',        type=str,               default=None) # glob support
parser.add_argument('--url',                type=str,               default="s3://sliderule-public")
parser.add_argument('--retries',            type=int,               default=3)
parser.add_argument('--report_only',        action='store_true',    default=False)
args,_ = parser.parse_known_args()

# Initialize Constants
RELEASE = "001"
VERSION = "01"

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Setup Log File
LOG_FORMAT = '%(created)f %(levelname)-5s [%(filename)s:%(lineno)5d] %(message)s'
log        = logging.getLogger(__name__)
logfmt     = logging.Formatter(LOG_FORMAT)
logfile    = logging.FileHandler(args.log_file)
logfile.setFormatter(logfmt)
log.addHandler(logfile)
log.setLevel(logging.INFO)
logfile.setLevel(logging.INFO)

# Initialize Concurrent Requests
rqst_q = queue.Queue()
if args.concurrency != None:
    concurrent_rqsts = args.concurrency
elif args.desired_nodes != None:
    concurrent_rqsts = args.desired_nodes
else:
    concurrent_rqsts = 1

# Creat S3 Client
s3_client = boto3.client("s3")

#
# List H5 Granules
#
def list_h5_granules(url, input_files):

    # get bucket and subfolder from url
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    subfolder = '/'.join(path.split("/")[1:])

    # read granules
    granules = []
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
                resource = obj['Key'].split("/")[-1]
                if resource.startswith("ATL24") and \
                  (resource.endswith("_" + RELEASE + "_" + VERSION + ".h5") or \
                   (not args.exclude_empty and resource.endswith(".empty"))):
                    granules.append(resource[:36].replace("ATL24", "ATL03") + ".h5")
        # check if more data is available
        is_truncated = response['IsTruncated']
        continuation_token = response.get('NextContinuationToken')
    print('') # new line

    # open and read input file
    rqsts = []
    already_processed = 0
    for input_file in input_files:
        with open(input_file, "r") as file:
            for line in file.readlines():
                granule = line.strip()
                if granule not in granules:
                    rqsts.append(granule)
                else:
                    already_processed += 1

    # Display Parameters
    print(f'Existing Granules:           {len(granules)}')
    print(f'Granules Already Processed   {already_processed}')
    print(f'Granules Left to Process:    {len(rqsts)}')

    # return list of granules to process
    return rqsts

#
# Process Request
#
def process_request(worker_id, granule):

    # Start Processing
    log.info(f'<{worker_id}> processing granule {granule} ...')

    # Build Output File Path
    output_filename = granule.replace("ATL03", "ATL24")[:-3] + "_" + RELEASE + "_" + VERSION + ".h5"

    # Set Parameters
    parms = {
        "asset": "icesat2",
        "resource": granule,
        "atl09_asset": "icesat2",
        "srt": icesat2.SRT_DYNAMIC,
        "cnf": "atl03_not_considered",
        "pass_invalid": True,
        "use_bathy_mask": True,
        "timeout": 600, # 10 minutes
        "classifiers": ["qtrees", "coastnet", "medianfilter", "cshelph", "bathypathfinder", "openoceanspp", "ensemble"],
        "output": {
            "asset": "sliderule-stage",
            "path": output_filename,
            "format": "h5",
            "open_on_complete": False,
            "with_checksum": False,
        }
    }

    # Make Request
    outfile = icesat2.atl24g(parms)
    log.info(f'<{worker_id}> finished granule {outfile}')

    # Parse Outfile Name
    path = outfile.split("s3://")[-1]
    bucket = path.split("/")[0]
    object_key = '/'.join(path.split("/")[1:])

    # Check Empty
    if outfile.endswith(".empty"):
        response = s3_client.get_object(Bucket=bucket, Key=object_key)
        content = response["Body"].read().decode("utf-8")
        return f'Empty - {content}'

    # Check Size
    try:
        response = s3_client.head_object(Bucket=bucket, Key=object_key)
        size = response["ContentLength"]
        if size <= 0:
            return f'Missing - size = {size}'
    except Exception as e:
        return "Missing"

    # Success
    return "Valid"

#
# Worker
#
def worker(worker_id):

    # While Queue Not Empty
    complete = False
    while not complete:

        # Check Server Availability
        while True:
            available_servers, _ = sliderule.update_available_servers()
            if (worker_id + 1) <= available_servers:
                break
            else:
                log.info(f'<{worker_id}> waiting for servers to become available')
                time.sleep(30)

        # Get Request
        try:
            granule = rqst_q.get(block=False)
        except Exception as e:
            # Handle No More Requests
            if rqst_q.empty():
                log.info(f'<{worker_id}> no more requests {e}')
                complete = True
            else:
                log.info(f'<{worker_id}> exception: {e}')
            time.sleep(5) # prevents a spin
            continue

        # Process Request
        valid = False
        status = "Unknown"
        attempts = args.retries
        while not valid and attempts > 0:
            attempts -= 1
            try:
                status = process_request(worker_id, granule)
                if status != "Missing":
                    valid = True
                else:
                    time.sleep(30) # slow it down
            except Exception as e:
                log.critical(f'failure on {granule}: {e}')
                time.sleep(30) # slow it down
                status = f'Error - {e}'

        # Display Status
        print(f'{granule}: {status}')

# Get List of Input Files
if args.input_files == None:
    list_of_input_files = [args.input_file]
else:
    list_of_input_files = glob.glob(args.input_files)

# Generate List of Unprocessed Granules
rqsts = list_h5_granules(args.url, list_of_input_files)

# Queue Processing Requests
for granule in rqsts:
    if "#" not in granule:
        rqst_q.put(granule)

# Check for Early Exit
if args.report_only:
    sys.exit(0)

# Initialize Python Client
sliderule.init(args.domain, verbose=False, loglevel=logging.INFO, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live, rethrow=True, log_handler=logfile)

# Start Workers
threads = [threading.Thread(target=worker, args=(worker_id,), daemon=True) for worker_id in range(concurrent_rqsts)]
for thread in threads:
    thread.start()
    time.sleep(args.startup_separation)
for thread in threads:
    thread.join()
log.info('all processing requests completed')
