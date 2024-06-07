# example: python utils/bathy_runner.py --domain <domain> --organization <org> --verbose --timeout 60000 --output_format parquet --classifiers medianfilter --plot 3

from sliderule import sliderule, icesat2
import argparse
import os

# Command Line Arguments
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--granule',            '-g',   type=str,               default="ATL03_20230213042035_08341807_006_02.h5") # ATL03_20190604044922_10220307_006_02.h5
parser.add_argument('--aoi',                '-a',   type=str,               default=None) # "tests/data/tarawa.geojson"
parser.add_argument('--track',              '-t',   type=int,               default=1)
parser.add_argument('--spots',              '-e',   type=int, nargs='+',    choices=range(1,7), default=[1,2,3,4,5,6])
parser.add_argument('--domain',             '-d',   type=str,               default="slideruleearth.io")
parser.add_argument('--organization',       '-o',   type=str,               default="bathy")
parser.add_argument('--desired_nodes',      '-n',   type=int,               default=None)
parser.add_argument('--time_to_live',       '-m',   type=int,               default=120)
parser.add_argument('--timeout',            '-x',   type=int,               default=800)
parser.add_argument('--loglvl',             '-l',   type=str,               default="INFO")
parser.add_argument('--output_file',        '-f',   type=str,               default="/tmp/ATL24_20190604044922_10220307_006_02")
parser.add_argument('--output_format',      '-r',   type=str,               default="hdf5")
parser.add_argument('--verbose',            '-v',   action='store_true',    default=False)
parser.add_argument('--cleanup',            '-u',   action='store_true',    default=False)
parser.add_argument('--generate_ndwi',      '-w',   action='store_true',    default=False)
parser.add_argument('--ignore_bathy_mask',  '-b',   action='store_true',    default=False)
parser.add_argument('--print_metadata',     '-i',   action='store_true',    default=False) # only available if [geo]parquet file format chosen
parser.add_argument('--classifiers',        '-c',   type=str, nargs='+',    default=["coastnet", "openoceans", "medianfilter", "cshelph", "bathypathfinder", "pointnet2", "ensemble"])
parser.add_argument('--plot',               '-p',   type=int, nargs='+',    choices=range(1,7), default=[])
args,_ = parser.parse_known_args()

# Initialize Organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# Initialize SlideRule Client
sliderule.init(args.domain, verbose=args.verbose, loglevel=args.loglvl, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# Build Output Filename
output_filename = args.output_file + "." + args.output_format

# Set Parameters
parms = { 
    "srt": icesat2.SRT_DYNAMIC,
    "cnf": "atl03_not_considered",
    "pass_invalid": True,
    "generate_ndwi": args.generate_ndwi,
    "use_bathy_mask": not args.ignore_bathy_mask,
    "timeout": args.timeout,
    "spots": args.spots,
    "classifiers": args.classifiers,
    "surfacefinder": {

    },
    "openoceans": {
        "use_ndwi": args.generate_ndwi
    },
    "output": {
        "path": output_filename, 
        "format": args.output_format, 
        "open_on_complete": args.output_format == "parquet" or args.output_format == "geoparquet", 
        "as_geo": args.output_format == "geoparquet" 
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