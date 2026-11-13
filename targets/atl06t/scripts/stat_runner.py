import os
import traceback
import boto3
import geopandas as gpd
from sliderule import pysr, s2tile

# runner
runner = pysr.Runner()
cell_id = runner.arguments[0]

# constants
OUTPUT_BUCKET = "sliderule-public"
OUTPUT_PREFIX = "atl06tv4"
TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally

# ##############
# Functions
# ##############

def build_stats(cell_id):
    gdf = gpd.read_parquet(f"s3://{OUTPUT_BUCKET}/{OUTPUT_PREFIX}/stats/ATL06T_S{cell_id:016X}.parquet")
    data_level = TILE_S2_CELL_LEVEL * 2

# ##############
# Main
# ##############

try:
    cell_level = s2tile.cell_level(cell_id)
    if cell_level == TILE_S2_CELL_LEVEL:
        pass
    else:
        child_cell_ids = s2tile.cell_children(cell_id)

except RuntimeError as e:

    runner.fail(f"Exception: {e}")

except Exception as e:

    runner.fail(f"Unhandled exception: {traceback.format_exc()}")

# finish
runner.finish()