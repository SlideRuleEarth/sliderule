# Imports
import sliderule
from sliderule import earthdata, h5, icesat2, gedi
import argparse
import logging

# Command Line Arguments
parser = argparse.ArgumentParser(description="""Subset granules""")
parser.add_argument('--benchmarks',     '-b',   nargs='+', type=str,    default=[])
parser.add_argument('--granule03',      '-p',   type=str,               default="ATL03_20181017222812_02950102_005_01.h5")
parser.add_argument('--granule06',      '-c',   type=str,               default="ATL06_20181017222812_02950102_005_01.h5")
parser.add_argument('--aoi',            '-a',   type=str,               default="tests/data/grandmesa.geojson")
parser.add_argument('--asset',          '-t',   type=str,               default="icesat2")
parser.add_argument('--domain',         '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   '-o',   type=str,               default="sliderule")
parser.add_argument('--desired_nodes',  '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',   '-l',   type=int,               default=120)
parser.add_argument('--verbose',        '-v',   action='store_true',    default=False)
parser.add_argument('--loglvl',         '-j',   type=str,               default="CRITICAL")
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Initialize Log Level
loglevel = logging.CRITICAL
if args.loglvl == "ERROR":
    loglevel = logging.ERROR
elif args.loglvl == "INFO":
    loglevel = logging.INFO
elif args.loglvl == "DEBUG":
    loglevel = logging.DEBUG

# Initialize SlideRule Client
sliderule.init(args.domain, verbose=args.verbose, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# Generate Region Polygon
region = sliderule.toregion(args.aoi)

# Display Results
def display_results(name, gdf):
    print(f'{name}: {len(gdf)} x {len(gdf.keys())}')
    print("    SlideRule Timing Profiles")
    for key in sliderule.profiles:
        print("        {:20} {:.6f} secs".format(key + ":", sliderule.profiles[key]))
    print("    ICESat2 Timing Profiles")
    for key in icesat2.profiles:
        print("        {:20} {:.6f} secs".format(key + ":", icesat2.profiles[key]))

# Benchmark ATL06 Ancillary
def atl06_ancillary():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
        "atl03_geo_fields": ["solar_elevation"]
    }
    return icesat2.atl06p(parms, args.asset, resources=[args.granule03])

# Benchmark ATL03 Ancillary
def atl03_ancillary():
    parms = {
        "poly":             region["poly"],
        "srt":              icesat2.SRT_LAND,
        "atl03_ph_fields":  ["ph_id_count"]
    }
    return icesat2.atl06p(parms, args.asset, resources=[args.granule03])

# Main
if __name__ == '__main__':
    # define benchmarks
    benchmarks = {
        "atl06anc": atl06_ancillary,
        "atl03anc": atl03_ancillary
    }
    # build list of benchmarks to run
    benchmarks_to_run = args.benchmarks
    if len(benchmarks_to_run) == 0:
        benchmarks_to_run = benchmarks.keys()
    # run benchmarks
    for benchmark in benchmarks_to_run:
        gdf = benchmarks[benchmark]()
        display_results(benchmark, gdf)
