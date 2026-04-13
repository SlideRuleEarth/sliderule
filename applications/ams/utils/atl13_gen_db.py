import os
import sys
import json
import time
import argparse
import tempfile
import duckdb
import geopandas as gpd

# -------------------------------------------
# command line arguments
# -------------------------------------------
parser = argparse.ArgumentParser(description="""ATL24""")
parser.add_argument('--atl13_shapefile',    type=str,               default="/data/ATL13/ATL13_Inland_Water_Body_Mask_v3_20191220.shp")
parser.add_argument('--atl13_mappings',     type=str,               default="/data/ATL13/atl13_mappings.json")
parser.add_argument('--atl13_granules',     type=str,               default="/data/ATL13/atl13_granules.parquet")
parser.add_argument('--atl13_refids_db',    type=str,               default="/data/atl13refids.db")
parser.add_argument('--atl13_granules_db',  type=str,               default="/data/atl13granules.db")
parser.add_argument('--test',               action='store_true',    default=False)
args,_ = parser.parse_known_args()

# -------------------------------------------
# convert shapefile to duckdb
# -------------------------------------------

# read shapefile into geodataframe
print(f'Reading shapefile {args.atl13_shapefile}')
gdf = gpd.read_file(args.atl13_shapefile)
gdf["ATL13refID"] = gdf["ATL13refID"].astype(int)
gdf.set_index("ATL13refID", inplace=True)

# read mappings
with open(args.atl13_mappings, "r") as file:
    print(f'Loading ATL13 mappings {args.atl13_mappings}')
    atl13_mappings = json.loads(file.read())

# add mappings to dataframe
gdf["granules"] = gdf["ATL13refID"].map(atl13_mappings)

# convert refids parquet file into duckdb file
parquet_filename = tempfile.mktemp()
print(f'Writing parquet file {parquet_filename}')
gdf.to_parquet(parquet_filename, index=True)
try:
    os.remove(args.atl13_refids_db)
    print(f'Replacing old refid database {args.atl13_refids_db}')
except:
    print(f'Creating new refid database {args.atl13_refids_db}')
db = duckdb.connect(args.atl13_refids_db)
db.execute(f"""
    INSTALL spatial;
    LOAD spatial;
    CREATE TABLE atl13refids AS
    SELECT *
    FROM '{parquet_filename}'
    ORDER BY ATL13refID;
    CREATE INDEX idx_geom ON atl24db USING RTREE(geometry);
""")
os.remove(parquet_filename)

# convert granules parquet file into duckdb file
try:
    os.remove(args.atl13_granules_db)
    print(f'Replacing old granule database {args.atl13_granules_db}')
except:
    print(f'Creating new granule database {args.atl13_granules_db}')
db = duckdb.connect(args.atl13_granules_db)
db.execute(f"""
    INSTALL spatial;
    LOAD spatial;
    CREATE TABLE atl13granules AS
    SELECT
        * EXCLUDE (begin_time),
        CAST(begin_time AS TIMESTAMP) AS begin_time
    FROM '{args.atl13_granules}'
    ORDER BY begin_time;
    CREATE INDEX idx_begin_time ON atl13granules(begin_time);
    CREATE INDEX idx_granule ON atl13granules(granule);
    CREATE INDEX idx_geom ON atl13granules USING RTREE(geometry);
""")

# finish up
print(f'Complete')

# -------------------------------------------
# run test
# -------------------------------------------

if args.test != None:

    # read body mask
    print(f'Loading ATL13 body mask... ', end='')
    sys.stdout.flush()
    start_time = time.perf_counter()
    atl13refids = duckdb.connect(args.atl13_refids_db)
    atl13refids.execute("LOAD spatial;")
    print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

    # test refid lookup
    entry_by_refid = 5952002394
    df = atl13refids.execute(f"""
        SELECT *
        FROM atl13refids
        WHERE ATL13refID == {entry_by_refid};
    """).df()
    print("REFID TEST", df)

    # test name lookup
    entry_by_name = "Caspian Sea"
    df = atl13refids.execute(f"""
        SELECT *
        FROM atl13refids
        WHERE Lake_name == '{entry_by_name}';
    """).df()
    print("NAME TEST", df)

    # test coordinate lookup
    entry_by_coord = [-86.79835088109307, 42.762733124439904]
    df = atl13refids.execute(f"""
        SELECT *
        FROM atl13refids
        WHERE ST_Contains(geometry, ST_Point({args.entry_by_coord[0]}, {args.entry_by_coord[1]}));
    """).df()
    print("COORD TEST", df)

    # describe database
    df = db.execute("DESCRIBE atl13refids").fetchdf()
    print("DESCRIBE", df)
