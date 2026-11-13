import os
import sys
import json
import argparse
import subprocess
import earthaccess
import boto3
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor, as_completed
from sliderule import sliderule

# #####################################
# Globals
# #####################################

# command line arguments
parser = argparse.ArgumentParser(description="""ATL06 Tile Builder""")
parser.add_argument('--domain',         type=str,               default="localhost") # passed to sliderule client session
parser.add_argument('--cluster',        type=str,               default=None) # passed to sliderule client session
parser.add_argument('--verbose',        action='store_true',    default=False) # passed to sliderule client session
parser.add_argument('--secrets_file',   type=str,               default=os.path.join(Path.home(), ".sliderule_secrets"))
parser.add_argument('--atl06_granules', type=str,               default="/data/ATL06/atl06_granules.txt")
parser.add_argument('--atl06_tiles',    type=str,               default="/data/ATL06/atl06_tiles.txt")
parser.add_argument('--atl06_cells',    type=str,               default="/data/ATL06/atl06_cells.txt") # cells to stat
parser.add_argument('--output_bucket',  type=str,               default="sliderule-public")
parser.add_argument('--output_prefix',  type=str,               default="atl06tv4")
parser.add_argument('--aoi',            type=str,               default="full_greenland")
parser.add_argument('--workers',        type=int,               default=8) # number of threads for partitioning, tiling, and reading
parser.add_argument('--list_granules',  action='store_true',    default=False) # list granules to process (uses aoi and writes atl06_granules)
parser.add_argument('--list_tiles',     action='store_true',    default=False) # list tiles to process (uses boto3 ls and writes atl06_tiles)
parser.add_argument('--partition',      action='store_true',    default=False) # create partitions from granules
parser.add_argument('--tile',           action='store_true',    default=False) # create tiles from partitions
parser.add_argument('--stat',           action='store_true',    default=False) # create stats from tiles
parser.add_argument('--credentials',    action='store_true',    default=False) # write a credential file needed by some runners
args = parser.parse_args()

# create sliderule client
if args.list_granules:
    session = sliderule.create_session(domain=args.domain, cluster=args.cluster, verbose=args.verbose, rqst_timeout=(10,300))

# create S3 client
s3 = boto3.client("s3")

# runner scripts
PARTITION_RUNNER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "partition_runner.py")
TILE_RUNNER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "tile_runner.py")
TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally

# areas of interest
aoi = {
    "antarctica":[
        {"lon": -151.86,    "lat": -87.51},
        {"lon": -136.64,    "lat": -86.99},
        {"lon": -146.35,    "lat": -86.27},
        {"lon": -159.34,    "lat": -86.68},
        {"lon": -151.86,    "lat": -87.51}
    ],
    "greenland": [
        {"lon": -27.20,    "lat": 73.48},
        {"lon": -27.20,    "lat": 74.08},
        {"lon": -29.75,    "lat": 74.08},
        {"lon": -29.75,    "lat": 73.48},
        {"lon": -27.20,    "lat": 73.48}
    ],
    "full_greenland": sliderule.toregion([
        {"lon": -30.6131574, "lat": 84.8588517},
        {"lon": -5.45407370, "lat": 81.3060796},
        {"lon": -20.6501184, "lat": 69.4284836},
        {"lon": -43.4932874, "lat": 58.9830380},
        {"lon": -53.8466301, "lat": 62.1053366},
        {"lon": -74.8423299, "lat": 78.1890152},
        {"lon": -62.7608926, "lat": 83.0481915},
        {"lon": -30.6131574, "lat": 84.8588517},
    ])["poly"]
}

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

# get the level of an s2 cell id from the position of its lowest set bit
def s2_cell_level(cell_id):
    lsb = cell_id & -cell_id
    return 30 - ((lsb.bit_length() - 1) >> 1)

# get the parent of an s2 cell id at the given level
def s2_cell_parent(cell_id):
    level = s2_cell_level(cell_id)
    lsb = 1 << (2 * (30 - level))
    return (cell_id & -lsb & 0xFFFFFFFFFFFFFFFF) | lsb

# #####################################
# Command Functions
# #####################################

# query cmr for granules
def list_atl06_granules(poly, output):
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

# generate stat files
def generate_stats(atl06_tiles):
    paths = [path.strip() for path in open(atl06_tiles, "r") if path.strip()]
    tiles = [path.split("/")[-2] for path in paths]
    cell_ids = [int(tile.lstrip("S"), 16) for tile in tiles]
    files = {}
    file_level = TILE_S2_CELL_LEVEL
    while file_level >= 0:
        files[file_level] = [f"S{cell_id:016X}" for cell_id in cell_ids]
        cell_ids = list({s2_cell_parent(cell_id) for cell_id in cell_ids})
        file_level -= 1
    return files

# run partition_runner.py on a single granule
def worker(argument, script):
    result_file = f"/tmp/{argument.split('/')[-1]}.json"
    if os.path.exists(result_file):
        os.remove(result_file)
    proc = subprocess.run([sys.executable, script, argument, result_file], capture_output=False, text=True)
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
    print(f"Processed {len(arguments) - len(failures)} of {len(arguments)} granules ({len(failures)} failed)")
    return results

# get temporary s3 credentials for the nsidc bucket
def get_credentials(secrets_file):
    auth = earthaccess.login()
    credentials = auth.get_s3_credentials(daac="NSIDC")
    with open(secrets_file, "w") as file:
        json.dump(credentials, file)
    print(f"Secrets written to {secrets_file}")

# #####################################
# Main
# #####################################

# route command
if args.list_granules:  list_atl06_granules(aoi[args.aoi], args.atl06_granules)
elif args.list_tiles:   list_atl06_tiles(args.output_bucket, args.output_prefix, args.atl06_tiles)
elif args.partition:    run_workers(args.atl06_granules, PARTITION_RUNNER, args.workers)
elif args.tile:         run_workers(args.atl06_tiles, TILE_RUNNER, args.workers)
elif args.stat:         d = generate_stats(args.atl06_tiles)
elif args.credentials:  get_credentials(args.secrets_file)
