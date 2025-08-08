import json
import argparse
from concurrent.futures import as_completed, ThreadPoolExecutor
from sliderule import sliderule, earthdata

# -------------------------------------------
# command line arguments
# -------------------------------------------

# create argument parser
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--domain',         type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   type=str,               default="developers")
parser.add_argument('--granule',        type=str,               default=None) # "ATL13_20250302152414_11692601_006_01.h5"
parser.add_argument('--input_file',     type=str,               default="/data/ATL13/atl13_granules.txt")
parser.add_argument('--mapping_file',   type=str,               default="/data/ATL13/atl13_mappings.json")
parser.add_argument('--concurrency',    type=int,               default=8)
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

atl13_id = 0
atl13_refids = {} # refid => [granule_id, ...]
atl13_granules = {} # granule_id => granule

# -------------------------------------------
# remote procedure to execute on sliderule
# -------------------------------------------

def remote_procedure(granule):
    return f"""
local json = require("json")

local asset = core.getbyname("icesat2-atl13")
local h5obj = h5.file(asset, "{granule}")

local column_gt1l = h5obj:readp("gt1l/atl13refid")
local column_gt1r = h5obj:readp("gt1r/atl13refid")
local column_gt2l = h5obj:readp("gt2l/atl13refid")
local column_gt2r = h5obj:readp("gt2r/atl13refid")
local column_gt3l = h5obj:readp("gt3l/atl13refid")
local column_gt3r = h5obj:readp("gt3r/atl13refid")

local function count(results, column)
    local values = column:unique()
    for k,_ in pairs(values) do
        results[tostring(k)] = true
    end
end

local results = {{}}
count(results, column_gt1l)
count(results, column_gt1r)
count(results, column_gt2l)
count(results, column_gt2r)
count(results, column_gt3l)
count(results, column_gt3r)

return json.encode(results)
"""

# -------------------------------------------
# worker
# -------------------------------------------

def worker (granule):
    if args.dryrun:
        return (granule, None)
    else:
        return (granule, sliderule.source("ace", remote_procedure(granule)))

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

# initialize list of granules to process
granules_to_process = {}
if args.granule != None:
    granules_to_process[args.granule] = atl13_id
    atl13_granules[atl13_id] = args.granule
else:
    with open(args.input_file, "r") as file:
        for line in file.readlines():
            filename = line.strip()
            granules_to_process[filename] = atl13_id
            atl13_granules[atl13_id] = filename
            atl13_id += 1

# ---------------------------
# run multiple workers
# ---------------------------

with ThreadPoolExecutor(max_workers=args.concurrency) as executor:
    futures = [executor.submit(worker, granule) for granule in granules_to_process]
    for future in as_completed(futures):
        granule, result = future.result()
        print(f'Processed {granule}: {len(result)} reference ids')
        try:
            for refid in result:
                if refid not in atl13_refids:
                    atl13_refids[refid] = set()
                atl13_refids[refid].add(granules_to_process[granule])
        except Exception as e:
            print(f'Error parsing results for {granule}: {e}')
            print(f'Returned: {result}')

# ---------------------------
# output atl13 mappings
# ---------------------------

# convert sets to list to make json serializable
for refid in atl13_refids:
    atl13_refids[refid] = list(atl13_refids[refid])

# write out as json
with open(args.mapping_file, "w") as file:
    file.write(json.dumps({"refids": atl13_refids, "granules": atl13_granules}))
