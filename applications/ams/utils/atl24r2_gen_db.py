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
parser.add_argument('--collection_file',    type=str,   default="data/atl24_v2_granule_collection.csv")
parser.add_argument('--parquet_file',       type=str,   default="data/atl24r2.parquet")
parser.add_argument('--db_file',            type=str,   default="data/atl24r2.db")
args,_ = parser.parse_known_args()

# -------------------------------------------
# load collection
# -------------------------------------------
print(f'Loading collection {args.collection_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
collection_df = gpd.pd.read_csv(args.collection_file, usecols=['granule', 'beam', 'season', 'bathy_photons', 'bathy_mean_depth', 'bathy_min_depth', 'bathy_max_depth', 'polygon', 'begin_time'])
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create geometry column
# -------------------------------------------
def getpoly(poly_str):
    poly_list = poly_str.split(" ")
    coord_list = [(poly_list[i+1],poly_list[i]) for i in range(0, len(poly_list), 2)]
    if len(coord_list) > 20:
        return Polygon(coord_list).buffer(0.01).simplify(0.01)
    else:
        return Polygon(coord_list)
print(f'Creating geometry column... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
collection_df["geometry"] = collection_df.apply(lambda row: getpoly(row['polygon']), axis=1)
del collection_df["polygon"] # remove polygon now that it is contained in the geometry column
collection_gdf = gpd.GeoDataFrame(collection_df, geometry='geometry', crs='EPSG:7912')
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create parquet database file
# -------------------------------------------
print(f'Writing parquet file {args.parquet_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
collection_gdf.to_parquet(args.parquet_file, index=True)
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
db.execute(f"""
    INSTALL spatial;
    LOAD spatial;
    CREATE TABLE atl24db AS
    SELECT
        * EXCLUDE (begin_time),
        CAST(begin_time AS TIMESTAMP) AS begin_time
    FROM '{args.parquet_file}'
    ORDER BY begin_time;
    CREATE INDEX idx_begin_time ON atl24db(begin_time);
    CREATE INDEX idx_granule ON atl24db(granule);
    CREATE INDEX idx_geom ON atl24db USING RTREE(geometry);
""")
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# display structure of duckdb database
# -------------------------------------------
df = db.execute("DESCRIBE atl24db").fetchdf()
print(df)
