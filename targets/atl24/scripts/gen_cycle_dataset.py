import argparse
from sliderule import sliderule

# command line arguments
parser = argparse.ArgumentParser(description="""ATL24 Platinum Run""")
parser.add_argument('cycle', type=int, metavar="<cycle>")
parser.add_argument('--prefix', type=str, default="data")
args = parser.parse_args()

# initialize client with longer timeout
session = sliderule.create_session(rqst_timeout=(10,360))

# get list of granules to process
print(f"Requesting granules for cycle {args.cycle}")
atl03_granules = sliderule.source("earthdata", {
    "asset": "icesat2",
    "cycle": args.cycle,
    "max_resources": 100000
}, session=session)
print(f"Retrieved {len(atl03_granules)} granules")
granules = [f"{granule}" for granule in atl03_granules]

# save cycles to file
output_filename = f"{args.prefix}/atl03_granules_cycle_{args.cycle}.txt"
print(f"Writing granules to {output_filename}")
with open(output_filename, "w") as file:
    for granule in granules:
        file.write(f"{granule}\n")
