import os
import json
import argparse
import duckdb
import boto3
import pyarrow as pa
import pyarrow.dataset as ds
import pyarrow.parquet as pq
import geopandas as gpd
from geopandas.io.arrow import _geopandas_to_arrow
from sliderule import sliderule

# #####################################
# Command Line Arguments
# #####################################

parser = argparse.ArgumentParser(description="""ATL24 Platinum Run""")
parser.add_argument('--domain',         type=str,   default="localhost")
parser.add_argument('--cluster',        type=str,   default=None)
parser.add_argument('--atl06_granules', type=str,   default="/data/ATL06/atl06_granules_greenland.json")
parser.add_argument('--output_bucket',  type=str,   default="sliderule-public")
parser.add_argument('--output_prefix',  type=str,   default="atl06t")
args = parser.parse_args()

# #####################################
# Globals
# #####################################

# create S3 client
s3 = boto3.client("s3")

# initialize SlideRule client
session = sliderule.create_session(domain=args.domain, cluster=args.cluster, verbose=True)

# read input granules
with open(args.atl06_granules, "r") as file:
    granules = json.load(file)

# #####################################
# Functions
# #####################################

# Assign Cell IDs
def assign_cell_ids(gdf, level=18):
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    geometry = gdf.geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
    coords = gpd.pd.DataFrame({"lon": geometry.x.to_numpy(), "lat": geometry.y.to_numpy()})
    connection.register("coords", coords)
    return connection.sql(f"SELECT s2_cell_parent(s2_cellfromlonlat(lon, lat), {level})::UBIGINT AS cell_id FROM coords").fetchnumpy()["cell_id"]

# Write Parquet
def write_parquet(gdf, out_path, target_bytes=5*1048576):
    """
    rows per row group adapt to the bytes actually written, so each group lands near target_bytes
    row groups are extended to the end of its last cell_id so no cell spans two groups
    """
    table = _geopandas_to_arrow(gdf, index=True)
    cell_idx = table.schema.get_field_index("cell_id")
    cell_ids = gdf.index
    with pa.OSFile(out_path, "wb") as sink:
        with pq.ParquetWriter(sink, table.schema, sorting_columns=[pq.SortingColumn(cell_idx)]) as writer:
            start, rows = 0, 50_000
            while start < table.num_rows:
                end = min(start + rows, table.num_rows)
                end = int(cell_ids.searchsorted(cell_ids[end - 1], side="right"))
                before = sink.tell()
                writer.write_table(table.slice(start, end - start), row_group_size=end - start)
                rows = max(1, int((end - start) * target_bytes / max(1, sink.tell() - before)))
                start = end

# #####################################
# Main
# #####################################

# for each granule
for granule in granules[:1]:

    # make processing request
    print(f"Processing granule {granule}")
    gdf = sliderule.run("atl06x", {}, resources=[granule], session=session)

    # assign cell ids and make it the index
    gdf["cell_id"] = assign_cell_ids(gdf, level=7)
    gdf = gdf.reset_index().set_index("cell_id").sort_index(kind="stable")

    # build level 7 sub-gdf
    for cell_id in gdf.index.unique():
        l7_gdf = gdf[gdf.index == cell_id]
        local_parquet_file = f"/tmp/{granule.replace('.h5', '')}.S{cell_id:016X}.parquet"
        remote_parquet_file = f"{args.output_prefix}/S{cell_id:016X}/{granule.replace('.h5', '.parquet')}"
        write_parquet(l7_gdf, local_parquet_file)
        s3.upload_file(local_parquet_file, args.output_bucket, remote_parquet_file)
        os.remove(local_parquet_file)
