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
parser.add_argument('--data_csv_file',      type=str,   default="/data/is2gt/data.csv")
parser.add_argument('--parquet_file',       type=str,   default="/data/is2gt/data.parquet")
parser.add_argument('--db_file',            type=str,   default="/data/is2gt.db")
args = parser.parse_args()

# -------------------------------------------
# load data
# -------------------------------------------
print(f'Loading data from {args.data_csv_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
dd = {"time": [], "lon": [], "lat": []}
with open(args.data_csv_file, "r") as file:
    for line in file.readlines():
        tokens = line.split(",")
        timestamp = tokens[2].split(" ")
        datestamp = timestamp[0].split("-")
        dd["time"].append(f"{int(datestamp[2]):04}-{int(datestamp[1]):02}-{int(datestamp[0]):02} {timestamp[1]}")
        dd["lon"].append(tokens[3])
        dd["lat"].append(tokens[4])
df = gpd.pd.DataFrame(dd)
print(f'read {len(df)} rows in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create geometry column
# -------------------------------------------
print(f'Creating geometry column... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
geometry = gpd.points_from_xy(df["lon"], df["lat"])
gdf = gpd.GeoDataFrame(df[["time"]], geometry=geometry, crs="EPSG:4326")
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create parquet database file
# -------------------------------------------
print(f'Writing parquet file {args.parquet_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
gdf.to_parquet(args.parquet_file, index=True)
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
    CREATE TABLE is2gtdb AS
    SELECT
        * EXCLUDE (time),
        CAST(time AS TIMESTAMP) AS time
    FROM '{args.parquet_file}'
    ORDER BY time;
    CREATE INDEX idx_time ON is2gtdb(time);
    CREATE INDEX idx_geom ON is2gtdb USING RTREE(geometry);
""")
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# display structure of duckdb database
# -------------------------------------------
df = db.execute("DESCRIBE is2gtdb").fetchdf()
print(df)
