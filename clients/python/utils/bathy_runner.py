# example: python utils/bathy_runner.py --domain <domain> --organization <org> --verbose --timeout 60000 --output_format hdf5 --output_as_sdp --output_file stage

from sliderule import sliderule, icesat2
import configparser
import argparse
import os

# Command Line Arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--granule',            '-g',   type=str,               default="ATL03_20230213042035_08341807_006_02.h5")
parser.add_argument('--aoi',                '-a',   type=str,               default=None) # "tests/data/tarawa.geojson"
parser.add_argument('--track',              '-t',   type=int,               default=1)
parser.add_argument('--spots',                      type=int, nargs='+',    choices=range(1,7), default=[1,2,3,4,5,6])
parser.add_argument('--domain',             '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',       '-o',   type=str,               default="bathy")
parser.add_argument('--desired_nodes',      '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',       '-m',   type=int,               default=120)
parser.add_argument('--timeout',                    type=int,               default=800)
parser.add_argument('--loglvl',             '-l',   type=str,               default="INFO")
parser.add_argument('--output_file',        '-f',   type=str,               default=None) # default to "/tmp/ATL24_<rest_of_input_granule>.<format>"
parser.add_argument('--output_format',              type=str,               default="hdf5")
parser.add_argument('--verbose',            '-v',   action='store_true',    default=False)
parser.add_argument('--cleanup',                    action='store_true',    default=False)
parser.add_argument('--asset',                      type=str,               default="icesat2")
parser.add_argument('--asset09',                    type=str,               default="icesat2")
parser.add_argument('--generate_ndwi',              action='store_true',    default=False)
parser.add_argument('--ignore_bathy_mask',          action='store_true',    default=False)
parser.add_argument('--print_metadata',             action='store_true',    default=False) # only available if [geo]parquet file format chosen
parser.add_argument('--with_checksum',              action='store_true',    default=False)
parser.add_argument('--output_as_sdp',              action='store_true',    default=False)
parser.add_argument('--classifiers',                type=str, nargs='+',    default=["qtrees", "coastnet", "openoceans", "medianfilter", "cshelph", "bathypathfinder", "pointnet2", "ensemble"])
parser.add_argument('--plot',               '-p',   type=int, nargs='+',    choices=range(1,7), default=[])
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Initialize SlideRule Client
sliderule.init(args.domain, verbose=args.verbose, loglevel=args.loglvl, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# Build Output File Path (and credentials if necessary)
if args.output_file == None:
    output_filename         = "/tmp/" + args.granule.replace("ATL03", "ATL24").replace(".h5", "." + args.output_format)
    credentials             = None
elif args.output_file == "stage":
    output_filename         = "s3://sliderule/data/ATL24/" + args.granule.replace("ATL03", "ATL24").replace(".h5", "." + args.output_format)
    home_directory          = os.path.expanduser('~')
    aws_credential_file     = os.path.join(home_directory, '.aws', 'credentials')
    config                  = configparser.RawConfigParser()
    config.read(aws_credential_file)
    credentials = {
        "aws_access_key_id": config.get('default', 'aws_access_key_id'),
        "aws_secret_access_key": config.get('default', 'aws_secret_access_key'),
        "aws_session_token": config.get('default', 'aws_session_token')
    }

# Set Parameters
parms = { 
    "asset": args.asset,
    "atl09_asset": args.asset09,
    "srt": icesat2.SRT_DYNAMIC,
    "cnf": "atl03_not_considered",
    "pass_invalid": True,
    "generate_ndwi": args.generate_ndwi,
    "use_bathy_mask": not args.ignore_bathy_mask,
    "timeout": args.timeout,
    "spots": args.spots,
    "output_as_sdp": args.output_as_sdp,
    "classifiers": args.classifiers,
    "openoceans": {
        "use_ndwi": args.generate_ndwi
    },
    "bathypathfinder": {
        "find_surface": False
    },
    "output": {
        "path": output_filename, 
        "format": args.output_format, 
        "open_on_complete": args.output_format == "parquet" or args.output_format == "geoparquet", 
        "as_geo": args.output_format == "geoparquet",
        "with_checksum": args.with_checksum,
        "region": "us-west-2",
        "credentials": credentials
    }
}

# Generate Region Polygon
if args.aoi:
    region = sliderule.toregion(args.aoi)
    parms["poly"] = region['poly']

# Make ATL24G Processing Request
df = icesat2.atl24g(parms, resource=args.granule)

# Plot Results
if len(args.plot):
    import matplotlib.pyplot as plt
    import numpy as np
    spots_to_plot = [int(s) for s in args.plot]
    pdf = df[df["spot"].isin(spots_to_plot)]
    plt.figure(figsize=[8,6])
    colors={41:['blue', 'surface'], 40:['red','bathy'], 0:['gray','other']}
    d0 = np.min(pdf["x_atc"])
    for class_val, color_name in colors.items():
        ii = pdf["class_ph"]==class_val
        plt.plot(pdf['x_atc'][ii]-d0, pdf['geoid_corr_h'][ii],'o',markersize=1, color=color_name[0], label=color_name[1])
    hl=plt.legend(loc=3, frameon=False, markerscale=5)
    plt.show()

# Clean Up Temporary Files
if args.cleanup:
    os.remove(output_filename)

# Display Metadata
if args.print_metadata:
    import pyarrow.parquet as pq
    import json
    parquet_file = pq.ParquetFile(output_filename)
    metadata = parquet_file.metadata
    metadata = metadata.metadata[b'sliderule'].decode('utf-8')
    metadata = json.loads(metadata)
    print(json.dumps(metadata, indent=4))