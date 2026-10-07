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
parser.add_argument('--domain',         type=str,               default="localhost")
parser.add_argument('--cluster',        type=str,               default=None)
parser.add_argument('--atl06_granules', type=str,               default="/data/ATL06/atl06_granules_greenland.json")
parser.add_argument('--output_bucket',  type=str,               default="sliderule-public")
parser.add_argument('--output_prefix',  type=str,               default="atl06t")
parser.add_argument('--partition',      action='store_true',    default=False)
parser.add_argument('--tile',           action='store_true',    default=False)
args = parser.parse_args()

# #####################################
# Globals
# #####################################

# create S3 client
s3 = boto3.client("s3")

# initialize SlideRule client
session = sliderule.create_session(domain=args.domain, cluster=args.cluster, verbose=True)

# #####################################
# Functions
# #####################################

# Split out bucket and key from url
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    key = '/'.join(path.split("/")[1:])
    return bucket, key

# list files in an s3 bucket
def list_bucket(url):
    filenames = []
    bucket, prefix = parse_url(url)
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
                filenames.append(f"{bucket}/{obj['Key']}")
        # check if more data is available
        is_truncated = response['IsTruncated']
        continuation_token = response.get('NextContinuationToken')
    return filenames


# list subfolders (one level deep) in an s3 bucket
def list_subfolders(url):
    subfolders = []
    bucket, prefix = parse_url(url)
    if prefix and not prefix.endswith("/"):
        prefix += "/"
    paginator = s3.get_paginator("list_objects_v2")
    # the "/" delimiter makes S3 group keys by their next path segment into CommonPrefixes
    for page in paginator.paginate(Bucket=bucket, Prefix=prefix, Delimiter="/"):
        for common_prefix in page.get("CommonPrefixes", []):
            subfolders.append(f"{bucket}/{common_prefix['Prefix']}")
    return subfolders

# assign cell IDs
def assign_cell_ids(gdf, level=18):
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    geometry = gdf.geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
    coords = gpd.pd.DataFrame({"lon": geometry.x.to_numpy(), "lat": geometry.y.to_numpy()})
    connection.register("coords", coords)
    return connection.sql(f"SELECT s2_cell_parent(s2_cellfromlonlat(lon, lat), {level})::UBIGINT AS cell_id FROM coords").fetchnumpy()["cell_id"]

# write optimized parquet
def write_optimized_parquet(gdf, output_file, target_bytes=5*1048576):
    """
    rows per row group adapt to the bytes actually written, so each group lands near target_bytes
    row groups are extended to the end of its last cell_id so no cell spans two groups
    """
    table = _geopandas_to_arrow(gdf, index=True)
    cell_idx = table.schema.get_field_index("cell_id")
    cell_ids = gdf.index
    with pa.OSFile(output_file, "wb") as sink:
        with pq.ParquetWriter(sink, table.schema, sorting_columns=[pq.SortingColumn(cell_idx)]) as writer:
            start, rows = 0, 50_000
            while start < table.num_rows:
                end = min(start + rows, table.num_rows)
                end = int(cell_ids.searchsorted(cell_ids[end - 1], side="right"))
                before = sink.tell()
                writer.write_table(table.slice(start, end - start), row_group_size=end - start)
                rows = max(1, int((end - start) * target_bytes / max(1, sink.tell() - before)))
                start = end

# write S2 partitioned parquets
def write_s2_partitions(level=7):
    with open(args.atl06_granules, "r") as file:
        granules = json.load(file)
    for granule in granules[:1]:
        print(f"Partitioning {granule}")
        gdf = sliderule.run("atl06x", {}, resources=[granule], session=session)
        gdf["cell_id"] = assign_cell_ids(gdf, level)
        gdf = gdf.reset_index().set_index("cell_id").sort_index(kind="stable")
        for cell_id in gdf.index.unique():
            l7_gdf = gdf[gdf.index == cell_id]
            local_parquet_file = f"/tmp/{granule.replace('.h5', '')}.S{cell_id:016X}.parquet"
            remote_parquet_file = f"{args.output_prefix}/partitions/S{cell_id:016X}/{granule.replace('.h5', '.parquet')}"
            l7_gdf.to_parquet(local_parquet_file, index=False)
            s3.upload_file(local_parquet_file, args.output_bucket, remote_parquet_file)
            os.remove(local_parquet_file)

# write ATL06 tiles
def write_atl06_tiles(level=18):
    subfolders = list_subfolders(f"s3://{args.output_bucket}/{args.output_prefix}/partitions/")
    for subfolder in subfolders:
        cell_id_str = subfolder.split("/")[-2]
        print(f"Tiling {cell_id_str}")
        filenames = list_bucket(f"s3://{subfolder}")
        gdfs = [gpd.read_parquet(f"s3://{filename}") for filename in filenames]
        if len(gdfs) > 0:
            gdf = gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
            gdf["cell_id"] = assign_cell_ids(gdf, level)
            gdf = gdf.reset_index().set_index("cell_id").sort_index(kind="stable")
            local_parquet_file = f"/tmp/ATL06T_{cell_id_str}.parquet"
            remote_parquet_file = f"{args.output_prefix}/tiles/ATL06T_{cell_id_str}.parquet"
            write_optimized_parquet(gdf, local_parquet_file)
            s3.upload_file(local_parquet_file, args.output_bucket, remote_parquet_file)
            os.remove(local_parquet_file)

# #####################################
# Main
# #####################################

if args.partition:  write_s2_partitions()
elif args.tile:     write_atl06_tiles()