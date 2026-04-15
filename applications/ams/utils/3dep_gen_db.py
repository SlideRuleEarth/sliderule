import os
import sys
import time
import argparse
import tempfile
import duckdb
import geopandas as gpd
from shapely.geometry import Polygon

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(description="""3DEP""")
parser.add_argument('--parquet_file',   type=str,   default="/data/3DEP/3dep_catalog_v3.parquet")
parser.add_argument('--db_file',        type=str,   default="/data/3dep.db")
args,_ = parser.parse_known_args()

# -------------------------------------------
# load original parquet file
# -------------------------------------------
print(f'Loading {args.parquet_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
gdf = gpd.read_parquet(args.parquet_file, columns=['id', 'proj:projjson', 'proj:transform', 'assets', 'collection', 'start_datetime', 'end_datetime', 'proj:code', 'proj:bbox', 'proj:shape', 'bbox', 'geometry'])
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create temporary parquet file
# -------------------------------------------
tmp_file = tempfile.mktemp() + ".parquet"
print(f'Writing parquet file {tmp_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
gdf.to_parquet(tmp_file, index=True)
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

    CREATE TABLE "3depdb" AS
    SELECT
        * EXCLUDE (start_datetime, end_datetime, "proj:projjson"),
        CAST(start_datetime AS TIMESTAMP) AS start_datetime,
        CAST(end_datetime AS TIMESTAMP) AS end_datetime,
        to_json("proj:projjson") AS "proj:projjson"
    FROM '{tmp_file}';

    CREATE INDEX idx_id ON "3depdb"(id);
    CREATE INDEX idx_geom ON "3depdb" USING RTREE(geometry);
""")
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# display structure of duckdb database
# -------------------------------------------
df = db.execute("""DESCRIBE "3depdb";""").fetchdf()
print(df)
