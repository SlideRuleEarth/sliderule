import os
import sys
import json
import argparse
import subprocess
import boto3
from concurrent.futures import ThreadPoolExecutor, as_completed
from sliderule import sliderule

# #####################################
# Globals
# #####################################

# create S3 client
s3 = boto3.client("s3")

# runner scripts
PARTITION_RUNNER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "partition_runner.py")
TILE_RUNNER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tile_runner.py")

# test site
greenland_poly = [
    {"lon": -27.20,    "lat": 73.48},
    {"lon": -27.20,    "lat": 74.08},
    {"lon": -29.75,    "lat": 74.08},
    {"lon": -29.75,    "lat": 73.48},
    {"lon": -27.20,    "lat": 73.48}
]

# #####################################
# Helper Functions
# #####################################

# Split out bucket and key from url
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    key = '/'.join(path.split("/")[1:])
    return bucket, key

# list subfolders (one level deep) in an s3 bucket
def list_subfolders(url):
    subfolders = []
    bucket, prefix = parse_url(url)
    if prefix and not prefix.endswith("/"):
        prefix += "/"
    paginator = s3.get_paginator("list_objects_v2")
    # the "/" delimiter makes S3 group keys by their next path segment into CommonPrefixes
    for page in paginator.paginate(Bucket=bucket, Prefix=prefix, Delimiter="/"):
        for common_prefix in page.get("CommonPrefixes", []):
            subfolders.append(f"{bucket}/{common_prefix['Prefix']}")
    return subfolders

# #####################################
# Command Functions
# #####################################

# query cmr for granules
def list_atl06_granules(poly, output, domain, cluster, verbose):
    session = sliderule.create_session(domain=domain, cluster=cluster, verbose=verbose, rqst_timeout=(10,300))
    granules = session.source("earthdata", {
        "asset": "icesat2-atl06",
        "poly": poly,
        "max_resources": 500000,
    }, rethrow=True)
    if isinstance(granules, list) and len(granules) > 0:
        print(f"Writing {len(granules)} granules to {output}")
        with open(output, "w") as file:
            for granule in granules:
                file.write(f"{granule}\n")
    else:
        print(f"Failed query for granules: {granules}")

# list ATL06 tiles
def list_atl06_tiles(bucket, prefix, output):
    subfolders = list_subfolders(f"s3://{bucket}/{prefix}/partitions/")
    with open(output, "w") as file:
        for subfolder in subfolders:
            file.write(f"{subfolder}\n")

# run partition_runner.py on a single granule
def worker(argument, script):
    result_file = f"/tmp/{argument.split('/')[-1]}.json"
    if os.path.exists(result_file):
        os.remove(result_file)
    proc = subprocess.run([sys.executable, script, argument, result_file], capture_output=True, text=True)
    try:
        with open(result_file, "r") as file:
            result = json.load(file)
        os.remove(result_file)
    except (OSError, json.JSONDecodeError) as e:
        result = {"status": False, "outputs": [], "messages": [f"No result ({e}), exit code {proc.returncode}: {proc.stderr[-2000:]}"]}
    return result

# run worker
def run_workers(input, script, workers):
    arguments = [argument.strip() for argument in open(input, "r") if argument.strip()]
    results = {}
    with ThreadPoolExecutor(max_workers=workers) as executor:
        futures = {executor.submit(worker, argument, script): argument for argument in arguments}
        for i, future in enumerate(as_completed(futures), start=1):
            argument = futures[future]
            result = future.result()
            results[argument] = result
            status = "OK" if result["status"] else "FAILED"
            print(f"[{i}/{len(arguments)}] {status} {argument}: {len(result['outputs'])} outputs")
            if not result["status"]:
                for message in result["messages"]:
                    print(f"    {message}")
    failures = [granule for granule, result in results.items() if not result["status"]]
    print(f"Partitioned {len(arguments) - len(failures)} of {len(arguments)} granules ({len(failures)} failed)")
    return results


# #####################################
# Main
# #####################################

if __name__ == "__main__":

    # command line arguments
    parser = argparse.ArgumentParser(description="""ATL06 Tile Builder""")
    parser.add_argument('--domain',         type=str,               default="localhost")
    parser.add_argument('--cluster',        type=str,               default=None)
    parser.add_argument('--atl06_granules', type=str,               default="/data/ATL06/atl06_granules.txt")
    parser.add_argument('--atl06_tiles',    type=str,               default="/data/ATL06/atl06_tiles.txt")
    parser.add_argument('--output_bucket',  type=str,               default="sliderule-public")
    parser.add_argument('--output_prefix',  type=str,               default="atl06tv2")
    parser.add_argument('--verbose',        action='store_true',    default=False)
    parser.add_argument('--workers',        type=int,               default=8) # number of threads for partitioning, tiling, and reading
    parser.add_argument('--list_granules',  action='store_true',    default=False) # create list of granules to process (uses aoi and writes atl06_granules)
    parser.add_argument('--list_tiles',     action='store_true',    default=False) # list all partitions
    parser.add_argument('--partition',      action='store_true',    default=False) # list all partitions
    parser.add_argument('--tile',           action='store_true',    default=False) # list all partitions
    args = parser.parse_args()

    # route command
    if args.list_granules:  list_atl06_granules(greenland_poly, args.atl06_granules, args.domain, args.cluster, args.verbose)
    elif args.list_tiles:   list_atl06_tiles(args.output_bucket, args.output_prefix, args.atl06_tiles)
    elif args.partition:    run_workers(args.atl06_granules, PARTITION_RUNNER, args.workers)
    elif args.tile:         run_workers(args.atl06_tiles, TILE_RUNNER, args.workers)
