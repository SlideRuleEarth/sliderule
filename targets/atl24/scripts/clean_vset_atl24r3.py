import argparse
import sys
import boto3
import earthaccess
from h5coro import h5coro, s3driver, logger

# ###############################
# Globals
# ###############################

# command line arguments
parser = argparse.ArgumentParser(description="""ATL24r3""")
parser.add_argument('--source_parquets',        type=str,               default="s3://sliderule-public/atl24r3/parquet")
parser.add_argument('--source_h5s',             type=str,               default="s3://sliderule-public/atl24r3/h5")
parser.add_argument('--validation_set',         type=str,               default="data/atl24r3_validation_set.txt")
parser.add_argument('--ut_test_set',            type=str,               default="data/atl24r3_ut_test_granules.txt")
parser.add_argument('--build_validation_set',   action='store_true',    default=False)
parser.add_argument('--quiet',                  action='store_true',    default=False)
args = parser.parse_args()

# create s3 client
s3 = boto3.client("s3")

# h5coro configuration
logger.config("CRITICAL")

# ###############################
# Helper Functions
# ###############################

# Display Raw
def display(s):
    if not args.quiet:
        sys.stdout.write(s)
        sys.stdout.flush()

# Parse URL into Bucket and Key
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    key = '/'.join(path.split("/")[1:])
    return bucket, key

# List Files in Bucket
def list_bucket(bucket, subfolder):
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
                resources.append(obj['Key'].split("/")[-1])
        # check if more data is available
        is_truncated = response['IsTruncated']
        continuation_token = response.get('NextContinuationToken')
    display(f"\nFound {len(resources)} resources\n")
    return resources

# Get Attributes
def get_attributes(bucket, key):
    response = s3.get_object(Bucket=bucket, Key=key, ChecksumMode="ENABLED")
    return {
        "size": response.get("ContentLength"),
        "sha256": response.get("ChecksumSHA256")
    }

# Get Groups
def get_groups(granule, credentials):
    granule_path = f"nsidc-cumulus-prod-protected/ATLAS/ATL03/{granule[30:33]}/{granule[6:10]}/{granule[10:12]}/{granule[12:14]}/{granule}"
    h5obj = h5coro.H5Coro(granule_path, s3driver.S3Driver, errorChecking=True, verbose=False, credentials=credentials)
    variables, attributes, groups = h5obj.list("/", w_attr=False)
    return groups

# Check Validation Set
def build_validation_set():
    # get credentials
    display(f"Authenticating to the NSIDC...\n")
    auth = earthaccess.login()
    s3_creds = auth.get_s3_credentials(daac="NSIDC")
    # open test set
    with open(args.ut_test_set, "r") as file:
        lines = file.readlines()
        granules = [line.strip() + ".h5" for line in lines if len(line) > 30]
    # process test set
    invalid_granules = []
    valid_granules = []
    for granule in granules:
        display(f"Opening {granule}...\n")
        status = True
        groups = {}
        try:
            groups = get_groups(granule, s3_creds)
            print(f"{granule} - {groups}")
        except:
            try:
                granule = granule.replace("_006_01.h5", "_006_02.h5")
                groups = get_groups(granule, s3_creds)
                print(f"{granule} - {groups}")
            except:
                status = False
        if status:
            valid_granules.append(granule)
        else:
            invalid_granules.append(granule)
    # list invalid granules
    display("\nInvalid granules:\n")
    for granule in invalid_granules:
        display(f"{granule}\n")
    # write valid granules
    display("\nWriting out valid granules:\n")
    with open(args.validation_set, "w") as file:
        for granule in valid_granules:
            display(f"{granule}\n")
            file.write(f"{granule}\n")
    # return valid granules
    return valid_granules

# ###############################
# Main
# ###############################

build_validation_set()