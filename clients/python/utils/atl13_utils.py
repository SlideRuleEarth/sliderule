import sys
import json
import argparse
import geopandas as gpd
from shapely.geometry import mapping

# -------------------------------------------
# command line arguments
# -------------------------------------------

# create argument parser
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--atl13_shapefile',    type=str,               default="/data/ATL13/bodyfile/ATL13_Inland_Water_Body_Mask_v3_20191220.shp")
parser.add_argument('--atl13_parquet',      type=str,               default="/data/ATL13/inland_water_body.parquet")
parser.add_argument('--atl13_mappings',     type=str,               default="/data/atl13_mappings.json")
parser.add_argument('--atl13_database',     type=str,               default="/data/atl13_database.parquet")
parser.add_argument('--convert_shapefile',  action='store_true',    default=False)
parser.add_argument('--entry_by_refid',     type=int,               default=None)
parser.add_argument('--entry_by_name',      type=str,               default=None)
parser.add_argument('--geometry_by_refid',  type=int,               default=None)
parser.add_argument('--geometry_by_name',   type=str,               default=None)
parser.add_argument('--generate_database',  action='store_true',    default=False)
args,_ = parser.parse_known_args()

# -------------------------------------------
# convert shapefile to parquet
# -------------------------------------------

if args.convert_shapefile:
    gdf = gpd.read_file(args.atl13_shapefile)
    gdf["ATL13refID"] = gdf["ATL13refID"].astype(int)
    gdf.set_index("ATL13refID", inplace=True)
    gdf.to_parquet(args.atl13_parquet_file, index=True)

# -------------------------------------------
# get entry by reference id
# -------------------------------------------
if args.entry_by_refid != None:
    gdf = gpd.read_parquet(args.atl13_parquet)
    print(gdf.loc[args.entry_by_refid].to_dict())

# -------------------------------------------
# get entry by reference id
# -------------------------------------------
if args.entry_by_name != None:
    gdf = gpd.read_parquet(args.atl13_parquet)
    gdf1 = gdf[gdf["Lake_name"] == args.entry_by_name]
    print(gdf1.iloc[0])

# -------------------------------------------
# get geometry by reference id
# -------------------------------------------
if args.geometry_by_refid != None:
    gdf = gpd.read_parquet(args.atl13_parquet)
    print(mapping(gdf.loc[args.geometry_by_refid].geometry))

# -------------------------------------------
# get geometry by reference id
# -------------------------------------------
if args.geometry_by_name != None:
    gdf = gpd.read_parquet(args.atl13_parquet)
    gdf1 = gdf[gdf["Lake_name"] == args.geometry_by_name]
    print(mapping(gdf1.iloc[0].geometry))

# -------------------------------------------
# combine body mask and mapping to create
# the full atl13 database
# -------------------------------------------
if args.generate_database:
    # read mappings
    with open(args.atl13_mappings, "r") as file:
        print(f'Loading ATL13 mappings...', end='')
        sys.stdout.flush()
        atl13_mappings = json.loads(file.read())
        print(f'complete.')
    # read body mask
    print(f'Loading ATL13 body mask...', end='')
    sys.stdout.flush()
    gdf = gpd.read_parquet(args.atl13_parquet)
    print(f'complete.')

# need to add column of granule ids
# then add metadata to the geodataframe that has the mapping of the ids to granule names
# then write out as the new atl13_database.parquet file
## then need to implement code in the manager that reads this file and serves requests
## then writeup documentation explaining what I did and how Arbitrary Code Execution works