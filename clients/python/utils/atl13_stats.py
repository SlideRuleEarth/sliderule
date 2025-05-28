import os
import argparse
from sliderule import sliderule, earthdata
from concurrent.futures import as_completed, ThreadPoolExecutor
import threading
import pandas as pd
import geopandas as gpd
import numpy as np

# -------------------------------------------
# command line arguments
# -------------------------------------------

# create argument parser
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--domain',         type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   type=str,               default="developers")
parser.add_argument('--granule',        type=str,               default=None) # "ATL13_20250302152414_11692601_006_01.h5"
parser.add_argument('--input_file',     type=str,               default="/data/atl13_granules.txt")
parser.add_argument('--output_file',    type=str,               default="/data/atl13_processed.txt")
parser.add_argument('--database_file',  type=str,               default="/data/atl13_database.csv")
parser.add_argument('--concurrency',    type=int,               default=6)
parser.add_argument('--query',          action='store_true',    default=False)  # query CMR to build list of filenames
parser.add_argument('--verbose',        action='store_true',    default=False)
parser.add_argument('--dryrun',         action='store_true',    default=False)
args,_ = parser.parse_known_args()

# initialize organization
if args.organization == "None":
    args.organization = None

# initialize client
if not args.dryrun:
    sliderule.init(args.domain, organization=args.organization, verbose=args.verbose)

# -------------------------------------------
# globals
# -------------------------------------------

atl13_database = {} # refid => [granule, ...]

# -------------------------------------------
# remote procedure to execute on sliderule
# -------------------------------------------

def remote_procedure(granule):
    return f"""
local json = require("json")

local asset = core.getbyname("icesat2-atl13")
local h5obj = h5.file(asset, "{granule}")
local column = h5obj:readp("gt1r/atl13refid")
local values = column:unique()

local results = {{}}
for k,v in pairs(values) do
    results[tostring(k)] = v
end

return json.encode(results)
"""

# -------------------------------------------
# worker
# -------------------------------------------

def worker (granule):
    if args.dryrun:
        return granule
    else:
        return sliderule.source("ace", remote_procedure(granule))

# -------------------------------------------
# query CMR for ATL13 granules
# -------------------------------------------

if args.query:
    earthdata.set_max_resources(20000)
    parms = {
        "asset": "icesat2-atl13",
        "t0": '2018-01-01T00:00:00Z'
    }
    granules = earthdata.search(parms)
    print(f'Writing {len(granules)} granules to {args.input_file}')
    with open(args.input_file, "w") as file:
        for granule in granules:
            file.write(f'{granule}\n')

# -------------------------------------------
# get list of ATL13 granules to process
# -------------------------------------------

# read list of granules already processed
granules_already_processed = {}
if os.path.exists(args.output_file):
    with open(args.output_file, "r") as file:
        for line in file.readlines():
            filename = line.strip()
            granules_already_processed[filename] = True

# initialize list of granules to process
granules_to_process = {}
if args.granule != None:
    granules_to_process[args.granule] = True
else:
    with open(args.input_file, "r") as file:
        for line in file.readlines():
            filename = line.strip()
            if filename not in granules_already_processed:
                granules_to_process[filename] = True

# ---------------------------
# run multiple workers
# ---------------------------

with ThreadPoolExecutor(max_workers=args.concurrency) as executor:
    futures = [executor.submit(worker, granule) for granule in granules_to_process]
    for future in as_completed(futures):
        result = future.result()
        print(result)
