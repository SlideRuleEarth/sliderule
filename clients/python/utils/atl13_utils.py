import sys
import json
import time
import argparse
import geopandas as gpd
from shapely.geometry import mapping, Point

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--atl13_shapefile',    type=str,               default="/data/ATL13/bodyfile/ATL13_Inland_Water_Body_Mask_v3_20191220.shp")
parser.add_argument('--atl13_parquet',      type=str,               default="/data/ATL13/inland_water_body.parquet")
parser.add_argument('--atl13_mappings',     type=str,               default="/data/ATL13/atl13_mappings.json")
parser.add_argument('--convert_shapefile',  action='store_true',    default=False)
parser.add_argument('--entry_by_refid',     type=int,               default=None) # 5952002394
parser.add_argument('--entry_by_name',      type=str,               default=None) # Caspian Sea
parser.add_argument('--entry_by_coord',     nargs='+', type=float,  default=[]) # -86.79835088109307 42.762733124439904
args,_ = parser.parse_known_args()

# -------------------------------------------
# load database
# -------------------------------------------
def load_database():
    # read mappings
    with open(args.atl13_mappings, "r") as file:
        print(f'Loading ATL13 mappings... ', end='')
        sys.stdout.flush()
        start_time = time.perf_counter()
        atl13_mappings = json.loads(file.read())
        print(f'completed in {time.perf_counter() - start_time:.2f} secs.')
    # read body mask
    print(f'Loading ATL13 body mask... ', end='')
    sys.stdout.flush()
    start_time = time.perf_counter()
    atl13_mask = gpd.read_parquet(args.atl13_parquet)
    print(f'completed in {time.perf_counter() - start_time:.2f} secs.')
    # return database
    return atl13_mask, atl13_mappings

# -------------------------------------------
# display_entry
# -------------------------------------------
def display_entry(refid):
    print("\nEntry\n-----\n", atl13_mask.loc[refid].to_dict(), "\n")
    print("Geometry\n--------\n", mapping(atl13_mask.loc[refid].geometry), "\n")
    print("Granules\n--------")
    for granule_id in atl13_mappings["refids"][str(refid)]:
        print(atl13_mappings["granules"][str(granule_id)])

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
    atl13_mask, atl13_mappings = load_database()
    display_entry(args.entry_by_refid)

# -------------------------------------------
# get entry by lake name
# -------------------------------------------
if args.entry_by_name != None:
    atl13_mask, atl13_mappings = load_database()
    atl13_mask = atl13_mask[atl13_mask["Lake_name"] == args.entry_by_name]
    display_entry(atl13_mask.index[0])

# -------------------------------------------
# get entry by coordinates
# -------------------------------------------
if len(args.entry_by_coord) == 2:
    atl13_mask, atl13_mappings = load_database()
    point = Point(args.entry_by_coord) # longitude, latitude
    atl13_mask = atl13_mask[atl13_mask.geometry.contains(point)]
    display_entry(atl13_mask.index[0])
