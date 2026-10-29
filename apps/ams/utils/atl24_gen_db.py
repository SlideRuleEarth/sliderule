import os
import sys
import time
import argparse
import duckdb
import geopandas as gpd
from shapely.geometry import Polygon

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--collection_file',    type=str,   default="/data/ATL24/atl24_v3_granule_collection.csv")
parser.add_argument('--db_file',            type=str,   default="/data/atl24r3.db")
args = parser.parse_args()

# -------------------------------------------
# load collection
# -------------------------------------------
print(f'Loading collection {args.collection_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
collection_df = gpd.pd.read_csv(args.collection_file, usecols=['name', 'beam', 'season', 'class_bathymetry', 'bathy_depth_mean', 'bathy_depth_min', 'bathy_depth_max', 'polygon', 'begin_time', 'end_time'])
collection_df["name"] = collection_df["name"].str.replace(r"\.parquet$", ".h5", regex=True)
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create geometry column
# -------------------------------------------
def getpoly(poly_str):
    # polygon string is "lon lat lon lat ..." (x y order)
    poly_list = [float(v) for v in poly_str.split()]
    return Polygon(zip(poly_list[0::2], poly_list[1::2]))
print(f'Creating geometry column... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
collection_df["geometry"] = collection_df["polygon"].map(getpoly)
del collection_df["polygon"] # remove polygon now that it is contained in the geometry column
collection_gdf = gpd.GeoDataFrame(collection_df, geometry='geometry', crs='EPSG:7912')
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create duckdb database file
# -------------------------------------------
try:
    os.remove(args.db_file)
    print(f'Replacing duckdb database {args.db_file}... ', end='')
except:
    print(f'Creating duckdb database {args.db_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
db = duckdb.connect(args.db_file)
db.register("collection_df", collection_gdf.to_wkb())  # geometry -> WKB bytes (BLOB)
db.execute("""
    INSTALL spatial;
    LOAD spatial;
    CREATE TABLE atl24db AS
    SELECT
        * EXCLUDE (begin_time, end_time, geometry),
        CAST(begin_time AS TIMESTAMP) AS begin_time,
        CAST(end_time AS TIMESTAMP) AS end_time,
        ST_GeomFromWKB(geometry) AS geometry
    FROM collection_df
    ORDER BY begin_time;
    CREATE INDEX idx_begin_time ON atl24db(begin_time);
    CREATE INDEX idx_geom ON atl24db USING RTREE(geometry);
""")
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# display structure of duckdb database
# -------------------------------------------
df = db.execute("DESCRIBE atl24db").fetchdf()
print(df)
