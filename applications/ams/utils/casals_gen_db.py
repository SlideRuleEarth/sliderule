import os
import sys
import time
import argparse
import duckdb
import boto3
import numpy as np
import geopandas as gpd
from shapely.geometry import Polygon, box
from sliderule import sliderule

# -------------------------------------------
# command line arguments
# -------------------------------------------

parser = argparse.ArgumentParser(description="""3DEP""")
parser.add_argument('--url',            type=str,               default="s3://casals-data/lidar/")
parser.add_argument('--filename',       type=str,               default=None) # "lidar/2024-11-12/casals_l1b_20241112T163941_001_02.h5"
parser.add_argument('--search_mask',    type=str,               default="/data/casals1b.geojson")
parser.add_argument('--tile_size',      type=int,               default=10000) # 10km
parser.add_argument('--parquet_file',   type=str,               default="/data/casals1b.parquet")
parser.add_argument('--db_file',        type=str,               default="/data/casals1b.db")
parser.add_argument('--domain',         type=str,               default="slideruleearth.io")
parser.add_argument('--organization',   type=str,               default="sliderule")
parser.add_argument('--desired_nodes',  type=int,               default=None)
parser.add_argument('--time_to_live',   type=int,               default=120)
parser.add_argument('--verbose',        action='store_true',    default=False)
parser.add_argument('--loglvl',         type=str,               default="CRITICAL")
args,_ = parser.parse_known_args()

# initialize organization
if args.organization == "None":
    args.organization = None
    args.desired_nodes = None
    args.time_to_live = None

# initialize SlideRule client
#sliderule.init(args.domain, verbose=args.verbose, loglevel=args.loglvl, organization=args.organization, desired_nodes=args.desired_nodes, time_to_live=args.time_to_live)

# -------------------------------------------
# process granules
# -------------------------------------------

# status display helper function
def display(s):
    sys.stdout.write(s)
    sys.stdout.flush()

# list bucket helper function
def list_bucket(url, suffix):
    filenames = []
    bucket = url.split("s3://")[-1].split("/")[0]
    prefix = "/".join(url.split("s3://")[-1].split("/")[1:])
    s3 = boto3.client("s3", region_name="us-west-2")
    is_truncated = True
    continuation_token = None
    while is_truncated:
        # make request
        if continuation_token:
            response = s3.list_objects_v2(Bucket=bucket, Prefix=prefix, ContinuationToken=continuation_token)
        else:
            response = s3.list_objects_v2(Bucket=bucket, Prefix=prefix)
        # parse contents
        if 'Contents' in response:
            for obj in response['Contents']:
                if obj['Key'].endswith(suffix):
                    filenames.append(f"{obj['Key']}")
        # check if more data is available
        is_truncated = response['IsTruncated']
        continuation_token = response.get('NextContinuationToken')
    return filenames

# traverse subfolders with data to build list of filenames
if args.filename == None:
    filenames = list_bucket(args.url, ".h5")
else:
    filenames = [args.filename]

# initialize collection
collection = {"granule": [], "begin_time": [], "geometry": []}

# process each granule
for filename in filenames:
    display(f"Processing granule s3://{filename}... ")
    start_time = time.perf_counter()
    gdf = sliderule.run("casals1bx", {}, resources=[filename])
    hull = gdf.geometry.union_all().convex_hull
    poly = Polygon(hull.exterior.coords)
    collection["granule"].append(filename)
    collection["begin_time"].append(gdf.index.min().isoformat())
    collection["geometry"].append(poly)
    display(f"{len(gdf)} rows in {time.perf_counter() - start_time:.2f} seconds\n")

# create geodataframe of collection
display(f"Creating geodataframe of collection... ")
start_time = time.perf_counter()
collection_gdf = gpd.GeoDataFrame(collection, geometry='geometry', crs='EPSG:9989')
print(f"completed in {time.perf_counter() - start_time:.2f} seconds")

# -------------------------------------------
# create search mask
# -------------------------------------------

display(f'Writing search mask {args.search_mask} for collection... ')
start_time = time.perf_counter()
gdf_proj = collection_gdf.to_crs("EPSG:3857")  # meters
minx, miny, maxx, maxy = gdf_proj.total_bounds
xs = np.arange(minx, maxx, args.tile_size)
ys = np.arange(miny, maxy, args.tile_size)
tiles = [box(x, y, x + args.tile_size, y + args.tile_size) for x in xs for y in ys]
tiles_gdf = gpd.GeoDataFrame(geometry=tiles, crs=gdf_proj.crs)
tiles_gdf = tiles_gdf[tiles_gdf.intersects(gdf_proj.geometry.union_all())]
tiles_gdf = tiles_gdf.to_crs("EPSG:4326")
tiles_gdf.to_file(args.search_mask, driver="GeoJSON")
print(f'completed in {time.perf_counter() - start_time:.2f} seconds')

# -------------------------------------------
# create database files
# -------------------------------------------

# create parquet database file
display(f'Writing parquet file {args.parquet_file} of collection... ')
start_time = time.perf_counter()
collection_gdf.to_parquet(args.parquet_file, index=True)
print(f'completed in {time.perf_counter() - start_time:.2f} seconds')

# create duckdb database file
try:
    os.remove(args.db_file)
    display(f'Replacing duckdb database {args.db_file}... ')
except:
    display(f'Creating duckdb database {args.db_file}... ')
start_time = time.perf_counter()
db = duckdb.connect(args.db_file)
db.execute(f"""
    INSTALL spatial;
    LOAD spatial;
    CREATE TABLE casals1bdb AS
    SELECT
        * EXCLUDE (begin_time),
        CAST(begin_time AS TIMESTAMP) AS begin_time
    FROM '{args.parquet_file}'
    ORDER BY begin_time;
    CREATE INDEX idx_begin_time ON casals1bdb(begin_time);
    CREATE INDEX idx_granule ON casals1bdb(granule);
    CREATE INDEX idx_geom ON casals1bdb USING RTREE(geometry);
""")
print(f'completed in {time.perf_counter() - start_time:.2f} seconds')

# display structure of duckdb database
df = db.execute("DESCRIBE casals1bdb").fetchdf()
print(df)
