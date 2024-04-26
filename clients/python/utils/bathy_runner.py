from sliderule import sliderule, icesat2
import argparse
import os

# Command Line Arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--output',         '-f',   type=str,               default=None)
parser.add_argument('--granule',        '-g',   type=str,               default="ATL03_20190604044922_10220307_006_02.h5")
parser.add_argument('--aoi',            '-a',   type=str,               default="tests/data/tarawa.geojson")
parser.add_argument('--track',          '-t',   type=int,               default=1)
parser.add_argument('--domain',         '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   '-o',   type=str,               default="bathy")
parser.add_argument('--desired_nodes',  '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',   '-m',   type=int,               default=120)
parser.add_argument('--verbose',        '-v',   action='store_true',    default=False)
parser.add_argument('--loglvl',         '-l',   type=str,               default="INFO")
parser.add_argument('--preserve',       '-p',   action='store_true',    default=False)
parser.add_argument('--serverfile',     '-s',   type=str,               default="/tmp/bathyfile.csv")
parser.add_argument('--return2client',  '-c',   action='store_true',    default=False)
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
    "cre": {
        "image": "openoceans", 
        "command": "/env/bin/python /usr/local/etc/oceaneyes.py",
        "parms": {
            "settings.json": {
                "var1": 1
            }
        }
    }
}
if args.return2client:
    parms["output"] = { 
        "path": args.serverfile, 
        "format": "csv", 
        "open_on_complete": True, 
        "as_geo": False 
    }

# Make ATL24G Processing Request
gdf = icesat2.atl24gp(parms, resources=[args.granule], keep_id=True)

# Write to CSV Local File
if(args.output != None):
    columns = [
        "index_ph", "time", "geoid_corr_h", "latitude", "longitude", "x_ph", "y_ph", "x_atc", "y_atc", 
        "sigma_along", "sigma_across", "ndwi", "yapc_score", "max_signal_conf", "quality_ph",
        "region", "track", "pair", "sc_orient", "rgt", "cycle", "utm_zone", 
        "background_rate", "solar_elevation", "wind_v", "pointing_angle", "extent_id" 
    ]
    gdf.to_csv(args.output, index=False, columns=columns)

# Clean Up Temporary Files
if args.return2client and not args.preserve:
    os.remove(args.serverfile)
