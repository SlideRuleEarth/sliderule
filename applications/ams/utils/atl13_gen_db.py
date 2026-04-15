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
parser.add_argument('--atl13_db',           type=str,               default="/data/atl13db.db")
parser.add_argument('--test',               action='store_true',    default=False)
args,_ = parser.parse_known_args()

# -------------------------------------------
# convert shapefile to duckdb
# -------------------------------------------

# read refid and granule mappings
with open(args.atl13_mappings, "r") as file:
    print(f'Loading ATL13 mappings {args.atl13_mappings}')
    atl13_mappings = json.load(file)

# read shapefile into geodataframe
print(f'Reading shapefile {args.atl13_shapefile}')
gdf = gpd.read_file(args.atl13_shapefile)
gdf["ATL13refID"] = gdf["ATL13refID"].astype(int) # convert refids to integers
gdf.set_index("ATL13refID", inplace=True) # make refids the index
gdf["granules"] = gdf["ATL13refID"].map(atl13_mappings["refids"]) # add refid mappings to dataframe

# open duckdb database
try:
    os.remove(args.atl13_db)
    print(f'Replacing old database {args.atl13_db}')
except:
    print(f'Creating new database {args.atl13_db}')
db = duckdb.connect(args.atl13_db)
db.execute(f"""
    INSTALL spatial;
    LOAD spatial;
""")

# convert refids parquet file into duckdb table
print(f'Writing refid table')
db.register("refid_gdf", gdf)
db.execute(f"""
    CREATE TABLE atl13refids AS
    SELECT *
    FROM refid_gdf
    ORDER BY ATL13refID;
    CREATE INDEX idx_geom ON atl24db USING RTREE(geometry);
""")
db.unregister("refid_gdf")

# convert granules parquet file into duckdb table
print(f'Writing granule table from {args.atl13_granules}')
db.execute(f"""
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

# convert granule mappings to duckdb table
print(f'Writing granule ids table from {parquet_filename}')
ids = list(atl13_mappings["granules"].keys())
granules = list(atl13_mappings["granules"].values())
granule_df = gpd.pd.DataFrame({"ids": ids, "granules": granules})
db.register("granule_df", granule_df)
db.execute(f"""
    CREATE TABLE atl13gids AS
    SELECT *
    FROM granule_df
    ORDER BY ids;
    CREATE INDEX idx_ids ON atl13gids(ids);
""")
db.unregister("granule_df")

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
    db = duckdb.connect(args.atl13_db)
    db.execute("LOAD spatial;")
    print(f'completed in {time.perf_counter() - start_time:.2f} secs.')

    # test refid lookup
    entry_by_refid = 5952002394
    df = db.execute(f"""
        SELECT *
        FROM atl13refids
        WHERE ATL13refID == {entry_by_refid};
    """).df()
    print("REFID TEST", df)

    # test name lookup
    entry_by_name = "Caspian Sea"
    df = db.execute(f"""
        SELECT *
        FROM atl13refids
        WHERE Lake_name == '{entry_by_name}';
    """).df()
    print("NAME TEST", df)

    # test coordinate lookup
    entry_by_coord = [-86.79835088109307, 42.762733124439904]
    df = db.execute(f"""
        SELECT *
        FROM atl13refids
        WHERE ST_Contains(geometry, ST_Point({args.entry_by_coord[0]}, {args.entry_by_coord[1]}));
    """).df()
    print("COORD TEST", df)

    # describe database
    df = db.execute("DESCRIBE atl13refids").fetchdf()
    print("DESCRIBE", df)
