import json
import traceback
import s3fs
import numpy as np
import pyarrow as pa
import pyarrow.parquet as pq
import geopandas as gpd
from sliderule import pysr, s2tile

# runner
runner = pysr.Runner()
cell_id = int(runner.arguments[0].lstrip("S"), 16)

# ##############
# Functions
# ##############


# ##############
# Main
# ##############

try:

    source_stat_filename = f"{s2tile.Atl06t.BUCKET}/{s2tile.Atl06t.PREFIX}/stats/ATL06S_S{cell_id:016X}.parquet"
    child_cell_ids = s2tile.cell_children(cell_id)

except RuntimeError as e:

    runner.fail(f"Exception: {e}")

except Exception as e:

    runner.fail(f"Unhandled exception: {traceback.format_exc()}")

# finish
runner.finish()