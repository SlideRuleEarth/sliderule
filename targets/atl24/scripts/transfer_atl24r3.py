import argparse
import base64
import os
import sys
import traceback
import boto3
import hashlib
import json
import uuid
import time
from enum import Enum
from datetime import datetime, timezone

# ###############################
# Globals
# ###############################

# Command Line Arguments
parser = argparse.ArgumentParser(description="""ATL24 Platinum Run""")
parser.add_argument('--stage',          type=str,               default="s3://sliderule/data/ATL24r3")
parser.add_argument('--cache',          type=str,               default="/data/ATL24/atl24r3_cache.json")
parser.add_argument('--data_version',   type=str,               default="003")
parser.add_argument('--transfer',       type=int,               default=0) # must provide in order to actually transfer
parser.add_argument('--batch_size',     type=int,               default=100, choices=range(1, 501))
parser.add_argument('--throttle',       type=float,             default=0.1) # seconds
parser.add_argument('--granule',        type=str,               default=None) # ATL24_20181014002954_02350105_007_01_003_01.h5
parser.add_argument('--test',           action='store_true',    default=False)
parser.add_argument('--verbose',        action='store_true',    default=False)
args = parser.parse_args()

# transfer parameters
collection              = "ATL24"
provider                = "ICESat-2_sliderule"
if args.test:
    response_stream_arn = "arn:aws:kinesis:us-west-2:941673577314:stream/nsidc-cumulus-uat-external_response"
    assume_role         = "arn:aws:iam::024284894447:role/nsidc-ops-uat_cross_provider_kinesis_role"
    notification_stream = "nsidc-ops-uat-ATLAS_sliderule_notification"
else:
    response_stream_arn = "arn:aws:kinesis:us-west-2:790840705381:stream/nsidc-ops-prod-ATLAS_sliderule_response"
    assume_role         = "arn:aws:iam::790840705381:role/nsidc-ops-prod_cross_provider_kinesis_role"
    notification_stream = "nsidc-ops-prod-ATLAS_sliderule_notification"

# create s3 client
s3 = boto3.client("s3")

# program status
exit_code = 0
num_granules_to_transfer = 0
records_success = 0
records_failure = 0

# ###############################
# Status Class
# ###############################

class Status(str, Enum):
    TX_READY        = "tx_ready"        # output located in s3 and attributes collected
    TX_INITIATED    = "tx_initiated"    # notification posted to the NSIDC stream
    TX_FAILED       = "tx_failed"       # notification could not be posted
    MISSING         = "missing"         # output could not be located in s3

# ###############################
# Helper Functions
# ###############################

# Parse URL into Bucket and Subfolder
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    subfolder = '/'.join(path.split("/")[1:])
    return bucket, subfolder

# Calculate Checksum
def calc_checksum(bucket, key):
    print(f"Warning! Calculating checksum for s3://{bucket}/{key}")
    response = s3.get_object(Bucket=bucket, Key=key)
    sha256 = hashlib.sha256()
    for chunk in iter(lambda: response["Body"].read(1024 * 1024), b""):
        sha256.update(chunk)
    return sha256.hexdigest()

# Get Size and SHA256 Checksum of S3 Object
def get_attributes(filename):
    bucket, subfolder = parse_url(args.stage)
    key = f"{subfolder}/{filename}"
    response = s3.head_object(Bucket=bucket, Key=key, ChecksumMode="ENABLED")
    checksum = response.get("ChecksumSHA256")
    if checksum and "-" not in checksum: # "-N" suffix is a composite (per-part) checksum, not the whole-object digest
        checksum = base64.b64decode(checksum).hex()
    else:
        checksum = calc_checksum(bucket, key)
    return {
        "name": filename,
        "path": f"{args.stage}/{filename}",
        "size": response["ContentLength"],
        "checksum": checksum
    }

# list S3 bucket
def list_bucket(suffix=".h5"):
    def display(s):
        sys.stdout.write(s)
        sys.stdout.flush()
    bucket, subfolder = parse_url(args.stage)
    resources = []
    is_truncated = True
    continuation_token = None
    while is_truncated:
        if continuation_token: response = s3.list_objects_v2(Bucket=bucket, Prefix=subfolder, ContinuationToken=continuation_token)
        else: response = s3.list_objects_v2(Bucket=bucket, Prefix=subfolder)
        display("#")
        # parse contents
        if 'Contents' in response:
            for obj in response['Contents']:
                if obj['Key'].endswith(suffix):
                    resources.append(obj['Key'].split("/")[-1])
        # check if more data is available
        is_truncated = response['IsTruncated']
        continuation_token = response.get('NextContinuationToken')
    display("\n")
    return resources

# ###############################
# Main
# ###############################

try:
    # Open cache of granules transferred
    cache = {}
    if os.path.exists(args.cache):
        with open(args.cache, "r") as file:
            cache = json.load(file)

    # Get granules to process
    all_available_granules = args.granule and [args.granule] or list_bucket()
    granules_to_process = [granule for granule in all_available_granules if (granule not in cache or cache[granule]["status"] != Status.TX_INITIATED)]
    print(f"Preparing to transfer {len(granules_to_process)} granule(s) out of {len(all_available_granules)} granule(s)")

    # Get attributes for each granule to process
    for i in range(len(granules_to_process)):
        granule = granules_to_process[i]
        if i % 10 == 0:
            sys.stdout.write(".")
            sys.stdout.flush()
        try:
            cache[granule] = {
                "h5": get_attributes(granule),
                "xml": get_attributes(granule.replace(".h5", ".iso.xml")),
                "status": Status.TX_READY
            }
        except Exception as e:
            print(f"Error! Missing output for {granule}: {e}")
            cache[granule] = {
                "status": Status.MISSING
            }
    sys.stdout.write("\n")
    sys.stdout.flush()

    # Initialize loop variables
    granules_to_transfer = [granule for granule, entry in cache.items() if (entry["status"] == Status.TX_READY and granule in granules_to_process)]
    num_granules_to_transfer = min(len(granules_to_transfer), args.transfer)
    previous_cred_refresh = 0.0 # previous time
    print(f"Transferring {num_granules_to_transfer} of {len(granules_to_transfer)} granules ready to be transferred")

    # Post records to stream
    for i in range(0, num_granules_to_transfer, args.batch_size):

        # Get kinesis client by assuming the role and getting credentials
        now = time.time()
        if (now - previous_cred_refresh) > (60 * 30): # 30 minutes
            print(f"Refreshing credentials for {assume_role}")
            previous_cred_refresh = now
            sts = boto3.client('sts')
            assumed_role = sts.assume_role(RoleArn=assume_role, RoleSessionName='AssumeRoleSession')
            credentials = assumed_role['Credentials'] # get temporary credentials
            kinesis = boto3.client(
                'kinesis',
                region_name='us-west-2',
                aws_access_key_id=credentials['AccessKeyId'],
                aws_secret_access_key=credentials['SecretAccessKey'],
                aws_session_token=credentials['SessionToken'])

        # Verbose status
        if args.verbose:
            print(f"Posting granules {i} to {i + args.batch_size} of {num_granules_to_transfer}")

        # Build batch of records
        batch = [{
            "Data": json.dumps({
                "version": 1.3,
                "submissionTime": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.000000"),
                "identifier": str(uuid.uuid4()),
                "collection": collection,
                "provider": provider,
                "responseStreamArn": response_stream_arn,
                "product": {
                    "name": cache[granule]["h5"]["name"],
                    "dataVersion": args.data_version,
                    "files": [
                        {
                            "name": cache[granule]["xml"]["name"],
                            "type": "metadata",
                            "uri": cache[granule]["xml"]["path"],
                            "checksumType": "SHA256",
                            "checksum": cache[granule]["xml"]["checksum"],
                            "size": cache[granule]["xml"]["size"],
                        },
                        {
                            "name": cache[granule]["h5"]["name"],
                            "type": "data",
                            "uri": cache[granule]["h5"]["path"],
                            "checksumType": "SHA256",
                            "checksum": cache[granule]["h5"]["checksum"],
                            "size": cache[granule]["h5"]["size"],
                        }
                    ]
                }
            }),
            "PartitionKey": granule
        } for granule in granules_to_transfer[i:min(i+args.batch_size, num_granules_to_transfer)]]

        # Post batch of records
        batch_response = kinesis.put_records(StreamName=notification_stream, Records=batch)
        for record, result, k in zip(batch, batch_response["Records"], range(len(batch))):
            granule = granules_to_transfer[i + k]
            if "ErrorCode" in result:   # e.g. ProvisionedThroughputExceededException
                # Retry post
                try:
                    time.sleep(args.throttle)
                    individual_response = kinesis.put_record(StreamName=notification_stream, Data=record["Data"], PartitionKey=granule)
                    cache[granule]["status"] = Status.TX_INITIATED
                    records_success += 1
                except Exception as e:
                    print(f"Error! Failed to put granule {granule}: {e}")
                    cache[granule]["status"] = Status.TX_FAILED
                    records_failure += 1
            else:
                # Success
                cache[granule]["status"] = Status.TX_INITIATED
                records_success += 1

except Exception:

    # Status Failure
    print(f"Error! Unhandled exception:\n{traceback.format_exc()}")
    exit_code = 1

finally:

    # Status
    print(f"Finished transferring {num_granules_to_transfer} records: {records_success} succeeded, {records_failure} failed.")

    # Save Database
    if not args.test:
        tmp_filename = f"{args.cache}.tmp"
        with open(tmp_filename, "w") as file:
            json.dump(cache, file, indent=2)
        os.replace(tmp_filename, args.cache)

sys.exit(exit_code)
