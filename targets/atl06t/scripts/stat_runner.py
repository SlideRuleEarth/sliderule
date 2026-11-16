import json
import traceback
import s3fs
import boto3
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq
import geopandas as gpd
from sliderule import pysr, s2tile

# runner
runner = pysr.Runner()
cell_id = int(runner.arguments[0].lstrip("S"), 16)

# create S3 client
s3 = boto3.client("s3")

# ##############
# Functions
# ##############

# min, max, and mean of each column (and of the geometry's x and y as lon and lat) for the rows of a group of
# stat_level cells, along with the number of rows and the first and last row group they were read from
def aggregate_stats(df, columns):
    grouped = df.groupby("cell_id")
    stats = grouped[columns].agg(["min", "max", "mean"])
    stats.columns = [f"{column}_{stat}" for column, stat in stats.columns]
    stats["row_count"] = grouped.size()
    stats["row_group_start"] = grouped["row_group"].min()
    stats["row_group_end"] = grouped["row_group"].max()
    return stats

# build stat_level cell statistics from the segment level rows of a tile, indexed by the stat_level cell id;
# the tile is read one row group at a time, and since its rows are sorted by cell_id each stat_level cell is
# a contiguous run of rows, so the last cell of a row group is held back until the next one shows where it ends
def build_stats(source_tile_filename, stat_level):
    tile = pq.ParquetFile(source_tile_filename, filesystem=s3fs.S3FileSystem())
    geo_metadata = json.loads(tile.schema_arrow.metadata[b"geo"])
    geometry_column = geo_metadata["primary_column"]
    # per the geoparquet spec a missing crs means OGC:CRS84 and a null crs means unknown
    crs = geo_metadata["columns"][geometry_column].get("crs", "OGC:CRS84")
    if isinstance(crs, dict):
        crs = json.dumps(crs)
    columns = [field.name for field in tile.schema_arrow if field.name != "cell_id" and \
               (pa.types.is_integer(field.type) or pa.types.is_floating(field.type) or \
                pa.types.is_boolean(field.type) or pa.types.is_timestamp(field.type))]
    lsb = np.uint64(1 << (2 * (30 - stat_level)))
    stats, pending = [], None
    for row_group in range(tile.num_row_groups):
        table = tile.read_row_group(row_group, columns=["cell_id", geometry_column, *columns])
        df = table.drop_columns(["cell_id", geometry_column]).to_pandas()
        geometry = gpd.GeoSeries.from_wkb(table.column(geometry_column).to_numpy(zero_copy_only=False), crs=crs)
        geometry = geometry.to_crs("EPSG:4326") # same reprojection tile_runner uses to assign cell ids
        df["lon"], df["lat"] = geometry.x.to_numpy(), geometry.y.to_numpy()
        del geometry
        # parent of each segment level cell at stat_level
        df["cell_id"] = (table.column("cell_id").to_numpy().view(np.uint64) & ~(lsb - np.uint64(1))) | lsb
        df["row_group"] = row_group
        if pending is not None:
            df = gpd.pd.concat([pending, df], ignore_index=True)
        if row_group < tile.num_row_groups - 1:
            # the last cell may continue into the next row group
            last = int(df["cell_id"].to_numpy().searchsorted(df["cell_id"].iloc[-1], side="left")) if len(df) > 0 else 0
            pending, df = df.iloc[last:], df.iloc[:last]
        if len(df) > 0:
            stats.append(aggregate_stats(df, ["lon", "lat", *columns]))
    if len(stats) == 0:
        return None
    return gpd.pd.concat(stats)


# ##############
# Main
# ##############

try:

    # read in the source tile file
    source_tile_filename = f"{s2tile.Atl06t.BUCKET}/{s2tile.Atl06t.PREFIX}/tiles/ATL06T_S{cell_id:016X}.parquet"
    df = build_stats(source_tile_filename, s2tile.Atl06t.TILE_S2_CELL_LEVEL*2)
    if df != None:
        # output the stat tile file
        output_stat_filename = f"{s2tile.Atl06t.PREFIX}/stats/ATL06S_S{cell_id:016X}.parquet"
        local_parquet_file = f"/tmp/ATL06S_S{cell_id:016X}.parquet"
        df.to_parquet(local_parquet_file)
        s3.upload_file(local_parquet_file, s2tile.Atl06t.BUCKET, output_stat_filename)
        runner.output(f"s3://{s2tile.Atl06t.BUCKET}/{output_stat_filename}")

except RuntimeError as e:

    runner.fail(f"Exception: {e}")

except Exception as e:

    runner.fail(f"Unhandled exception: {traceback.format_exc()}")

# finish
runner.finish()