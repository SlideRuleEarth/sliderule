import os
import sys
import json
import time
import duckdb
import traceback
import boto3
import pyarrow as pa
import pyarrow.parquet as pq
import geopandas as gpd
from datetime import datetime, timezone
from geopandas.io.arrow import _geopandas_to_arrow

# arguments
partition = sys.argv[1]
result_file = sys.argv[2]

# constants
SEGMENT_S2_CELL_LEVEL = 18 # ~1150 m^2
ROW_GROUP_TARGET_SIZE_MB = 2 # MB
OUTPUT_BUCKET = "sliderule-public"
OUTPUT_PREFIX = "atl06tv2"

# create S3 client
s3 = boto3.client("s3")

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

# assign cell IDs
def assign_cell_ids(gdf, level=18):
    connection = duckdb.connect()
    connection.sql("INSTALL geography FROM community; LOAD geography;")
    geometry = gdf.geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
    coords = gpd.pd.DataFrame({"lon": geometry.x.to_numpy(), "lat": geometry.y.to_numpy()})
    connection.register("coords", coords)
    return connection.sql(f"SELECT s2_cell_parent(s2_cellfromlonlat(lon, lat), {level})::UBIGINT AS cell_id FROM coords").fetchnumpy()["cell_id"]

# write optimized parquet
def write_optimized_parquet(gdf, output_file, target_bytes=ROW_GROUP_TARGET_SIZE_MB*1048576):
    """
    rows per row group adapt to the bytes actually written, so each group lands near target_bytes
    row groups are extended to the end of its last cell_id so no cell spans two groups
    """
    table = _geopandas_to_arrow(gdf, index=True)
    cell_idx = table.schema.get_field_index("cell_id")
    cell_ids = gdf.index
    floats = [f.name for f in table.schema if pa.types.is_floating(f.type)]
    with pa.OSFile(output_file, "wb") as sink:
        with pq.ParquetWriter(sink, table.schema,
                              sorting_columns=[pq.SortingColumn(cell_idx)],
                              compression="zstd",
                              # byte_stream_split only takes effect on columns without dictionary encoding
                              use_dictionary=[name for name in table.column_names if name not in floats],
                              use_byte_stream_split=floats,
                              write_statistics=["cell_id", "time_ns"]) as writer:
            start, rows = 0, 50_000
            while start < table.num_rows:
                end = min(start + rows, table.num_rows)
                end = int(cell_ids.searchsorted(cell_ids[end - 1], side="right"))
                before = sink.tell()
                writer.write_table(table.slice(start, end - start), row_group_size=end - start)
                rows = max(1, int((end - start) * target_bytes / max(1, sink.tell() - before)))
                start = end

# ##############
# Main
# ##############

result = {
    "status": True,
    "build": "local",
    "start": time.time(),
    "outputs": [],
    "messages": []
}

try:

    cell_id_str = partition.split("/")[-2]
    filenames = list_bucket(f"s3://{partition}")
    gdfs = [gpd.read_parquet(f"s3://{filename}") for filename in filenames]

    if len(gdfs) > 0:

        gdf = gpd.GeoDataFrame(gpd.pd.concat(gdfs, ignore_index=True), crs=gdfs[0].crs)
        result["messages"].append(f"{len(gdf)} rows from {len(filenames)} files")
        gdf["cell_id"] = assign_cell_ids(gdf, SEGMENT_S2_CELL_LEVEL)
        gdf = gdf.set_index("cell_id").sort_index(kind="stable")
        years = gpd.pd.to_datetime(gdf["time_ns"].to_numpy(), unit="ns", utc=True).year.to_numpy()

        for year in range(2018, datetime.now(timezone.utc).year + 1):
            year_gdf = gdf[years == year]
            if len(year_gdf) == 0: continue
            local_parquet_file = f"/tmp/ATL06T_{year}_{cell_id_str}.parquet"
            remote_parquet_file = f"{OUTPUT_PREFIX}/tiles/ATL06T_{year}_{cell_id_str}.parquet"
            write_optimized_parquet(year_gdf, local_parquet_file)
            s3.upload_file(local_parquet_file, OUTPUT_BUCKET, remote_parquet_file)
            result["outputs"].append(remote_parquet_file)
            result["messages"].append(f"{len(year_gdf)} rows written for {year}")
            os.remove(local_parquet_file)

    else:

        result["status"] = False
        result["messages"].append("no files")

except Exception as e:

    # errors
    result["status"] = False
    result["messages"].append(f"Unhandled exception: {traceback.format_exc()}")


# finish
result["stop"] = time.time()
with open(result_file, "w") as file:
    json.dump(result, file)