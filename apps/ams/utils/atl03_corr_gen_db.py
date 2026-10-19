import os
import sys
import time
import argparse
import duckdb
import geopandas as gpd

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(description="""ATL03 Corrections""")
parser.add_argument('--csv_file',   type=str,   default="/data/ATL03/conus_perbeam_corrections_draft.csv")
parser.add_argument('--db_file',    type=str,   default="/data/atl03.db")
args = parser.parse_args()

# -------------------------------------------
# load original parquet file
# -------------------------------------------
print(f'Loading {args.csv_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
gdf = gpd.pd.read_csv(args.csv_file, usecols=["granule", "beam", "shift_N_m", "shift_E_m", "shift_U_m"])
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# create duckdb database file
# -------------------------------------------
try:
    os.remove(args.db_file)
    print(f'Replacing duckdb database {args.db_file}... ', end='')
except FileNotFoundError:
    print(f'Creating duckdb database {args.db_file}... ', end='')
sys.stdout.flush()
start_time = time.perf_counter()
db = duckdb.connect(args.db_file)
# keep each granule's rows contiguous so zonemaps can prune row groups as the table grows
db.execute("""
    CREATE TABLE "atl03db" AS
    SELECT granule, beam, shift_N_m, shift_E_m, shift_U_m
    FROM gdf
    ORDER BY granule, beam;

    CREATE INDEX idx_granule ON "atl03db"(granule);
""")
print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

# -------------------------------------------
# display structure of duckdb database
# -------------------------------------------
df = db.execute("""DESCRIBE "atl03db";""").fetchdf()
print(df)
db.close()
