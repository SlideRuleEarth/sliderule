from sliderule import sliderule, icesat2
import argparse
import os

# Command Line Arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--granule',        '-g',   type=str,               default="ATL03_20190604044922_10220307_006_02.h5")
parser.add_argument('--aoi',            '-a',   type=str,               default="tests/data/tarawa.geojson")
parser.add_argument('--track',          '-t',   type=int,               default=1)
parser.add_argument('--spots',          '-e',   type=int, nargs='+',    choices=range(1,7), default=[1,2,3,4,5,6])
parser.add_argument('--domain',         '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   '-o',   type=str,               default="bathy")
parser.add_argument('--desired_nodes',  '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',   '-m',   type=int,               default=120)
parser.add_argument('--timeout',        '-x',   type=int,               default=800)
parser.add_argument('--verbose',        '-v',   action='store_true',    default=False)
parser.add_argument('--loglvl',         '-l',   type=str,               default="INFO")
parser.add_argument('--preserve',       '-p',   action='store_true',    default=False)
parser.add_argument('--serverfile',     '-s',   type=str,               default="/tmp/ATL24_20190604044922_10220307_006_02.h5")
parser.add_argument('--return_inputs',  '-i',   action='store_true',    default=False)
parser.add_argument('--send_to_client', '-c',   action='store_true',    default=False)
parser.add_argument('--generate_ndwi',  '-w',   action='store_true',    default=False)
parser.add_argument('--use_bathy_mask', '-b',   action='store_true',    default=False)
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Initialize SlideRule Client
sliderule.init(args.domain, verbose=args.verbose, loglevel=args.loglvl, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# Generate Region Polygon
region = sliderule.toregion(args.aoi)

# Set Parameters
parms = { 
    "poly": region['poly'],
    "srt": icesat2.SRT_DYNAMIC,
    "cnf": "atl03_not_considered",
    "pass_invalid": True,
    "generate_ndwi": args.generate_ndwi,
    "use_bathy_mask": args.use_bathy_mask,
    "return_inputs": args.return_inputs,
    "timeout": args.timeout,
    "spots": args.spots,
    "openoceans": {
        "res_along_track": 10,
        "res_z": 0.2,
        "window_size": 11,
        "range_z": [-50, 30],
        "verbose": False,
        "photon_bins": False,
        "use_ndwi": args.generate_ndwi
    }
}
if args.send_to_client:
    parms["output"] = { 
        "path": args.serverfile, 
        "format": "hdf5", 
        "open_on_complete": False, 
        "as_geo": False 
    }

# Make ATL24G Processing Request
gdf = icesat2.atl24gp(parms, resources=[args.granule], keep_id=True)

# Clean Up Temporary Files
if args.send_to_client and not args.preserve:
    os.remove(args.serverfile)
