import traceback
import boto3
import s3fs
import numpy as np
import pandas as pd
from sliderule import pysr, s2tile

# runner
runner = pysr.Runner()
cell_id = int(runner.arguments[0].lstrip("S"), 16)

# create S3 client
s3 = boto3.client("s3")

# constants
STAT_LEVEL_DELTA = 7

# ##############
# Functions
# ##############

# row_count weighted mean of the source cells' means for each stat_level cell; a NaN mean (a source cell
# with no valid values for that column) gets no weight, and timestamps are averaged as integer nanoseconds
def weighted_mean(means, weights, cell_ids):
    is_time = pd.api.types.is_datetime64_any_dtype(means)
    if is_time:
        tz = means.dt.tz
        values = means.dt.tz_convert(None) if tz is not None else means
        values = values.to_numpy(dtype="datetime64[ns]").astype(np.int64).astype(np.float64)
        values[means.isna().to_numpy()] = np.nan
    else:
        values = means.to_numpy(dtype=np.float64, na_value=np.nan)
    valid = ~np.isnan(values)
    w = np.where(valid, weights.to_numpy(dtype=np.float64), 0.0)
    sums = pd.DataFrame({"num": np.where(valid, values, 0.0) * w, "den": w, "cell_id": cell_ids.to_numpy()}).groupby("cell_id").sum()
    result = sums["num"] / sums["den"]
    if is_time:
        result = pd.to_datetime(result.round(), unit="ns", utc=tz is not None)
        if tz is not None:
            result = result.dt.tz_convert(tz)
        result = result.astype(means.dtype)
    return result

# combine the stat files of the child cells into stats for each stat_level cell, indexed by the stat_level cell id
def stack_stats(source_stat_filenames, stat_level):
    fs = s3fs.S3FileSystem()
    # stat_runner writes no stat file for a tile with no data
    existing = [filename for filename in source_stat_filenames if fs.exists(filename)]
    if len(existing) == 0:
        return None
    src = pd.concat([pd.read_parquet(filename, filesystem=fs) for filename in existing])
    # stat files store cell_id as the index; it is kept as a separate array rather than inserted as a
    # column because inserting into the many-block concatenated frame fragments it
    source_cell_ids = src.index if "cell_id" not in src.columns else src.pop("cell_id")
    # parent of each source cell at stat_level
    lsb = np.uint64(1 << (2 * (30 - stat_level)))
    cell_ids = pd.Series((source_cell_ids.to_numpy(dtype=np.uint64) & ~(lsb - np.uint64(1))) | lsb, name="cell_id")
    src = src.reset_index(drop=True)
    grouped = src.groupby(cell_ids, sort=True)
    stats = {}
    for column in src.columns:
        if column.endswith("_min") or column == "row_group_start":
            stats[column] = grouped[column].min()
        elif column.endswith("_max") or column == "row_group_end":
            stats[column] = grouped[column].max()
        elif column.endswith("_mean"):
            stats[column] = weighted_mean(src[column], src["row_count"], cell_ids)
        elif column == "row_count":
            stats[column] = grouped[column].sum()
        else:
            raise RuntimeError(f"unexpected column in stat file: {column}")
    # build the frame all at once to avoid fragmentation
    return pd.DataFrame(stats)

# ##############
# Main
# ##############

try:

    # read input files into stacked dataframe
    child_cell_ids = s2tile.cell_children(cell_id)
    source_stat_filenames = [f"{s2tile.Atl06t.BUCKET}/{s2tile.Atl06t.PREFIX}/stats/ATL06S_S{child_cell_id:016X}.parquet" for child_cell_id in child_cell_ids]
    stat_level = s2tile.cell_level(cell_id) + STAT_LEVEL_DELTA
    df = stack_stats(source_stat_filenames, stat_level)

    # output the stat tile file
    if df is not None:
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