import os
import json
import traceback
import argparse
import boto3
from PIL import Image
import pandas as pd
import numpy as np
from matplotlib.colors import LogNorm
import matplotlib.cm as cm
from concurrent.futures import as_completed, ThreadPoolExecutor

# ########################
# Command Line Arguments
# ########################

parser = argparse.ArgumentParser(description="""sliderule python job runner""")
parser.add_argument('--result_output',  type=str,               default="/tmp/atl24r3_stats.txt")
parser.add_argument('--summary_file',   type=str,               default="/data/ATL24/atl24_v3_granule_collection.csv")
parser.add_argument('--grid_file',      type=str,               default="/data/web/sliderule_atl24_grid.png")
parser.add_argument('--fresh_start',    action='store_true',    default=False) # overwrites summary_file
parser.add_argument('--concurrency',    type=int,               default=20)
args = parser.parse_args()

# ########################
# Globals
# ########################

s3 = boto3.client("s3")

# ########################
# Functions
# ########################

def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    key = '/'.join(path.split("/")[1:])
    return bucket, key

def load_remote_file(url):
    bucket, key = parse_url(url)
    obj = s3.get_object(Bucket=bucket, Key=key)
    contents = obj["Body"].read().decode("utf-8")
    try:
        return json.loads(contents)
    except Exception:
        return contents

def add_result(summary, result):
    if not summary:
        if not args.fresh_start and os.path.exists(args.summary_file):
            summary = pd.read_csv(args.summary_file)
            summary = summary.to_dict('list')
        else:
            summary = {key: [] for key in result}
    for key in result:
        summary[key].append(result[key])
    return summary

def write_csv_file(summary, filename):
    df = pd.DataFrame(summary)
    df.to_csv(filename, index=False)

def add_table(grid, table):
    for y,x,cnt in zip(table["rows"], table["cols"], table["cnts"]):
        if x == 1440: x = 1439
        if y == 720: y = 719
        if(x < 1440 and x >= 0 and y < 720 and y >= 0):
            grid[y, x] += cnt
        else:
            print(f"Error gridding {y},{x} => {cnt}")

def write_map_file(grid, filename):
    data = grid.astype(np.float64)
    data[data == 0] = np.nan
    vmin = max(np.nanmin(data), 1.0)  # clamp min to 1 for log scale
    norm = LogNorm(vmin=vmin, vmax=np.nanmax(data)) # log-scale normalization to accentuate lower values
    mapped = cm.jet(norm(data))  # RGBA float array
    mapped = (mapped * 255).astype(np.uint8)
    mask = np.isnan(data) # make NaN pixels fully transparent
    mapped[mask] = [0, 0, 0, 0]
    img = Image.fromarray(np.flipud(mapped), mode='RGBA')
    img.save(filename) # save image
    print(f'Saved PNG {filename}: {img.size[0]}x{img.size[1]} pixels')

def worker(output):
    try:
        if output["status"] == "success":
            return load_remote_file(output['outputs'][0])
    except Exception as e:
        print(f"Unhandled exception: {e}")
        traceback.print_exc()
    return {}

# ########################
# Main
# ########################

# read in outputs to process
outputs = [json.loads(line) for line in open(args.result_output, "r") if line.strip()]

# initialize grid and summary
grid = np.zeros((720, 1440), dtype=np.uint32)
summary = {}

# execute workers
with ThreadPoolExecutor(max_workers=args.concurrency) as executor:
    futures = [executor.submit(worker, output) for output in outputs]
    for future in as_completed(futures):
        result = future.result()
        add_table(grid, result["grid"])
        for spot in ["1","2","3","4","5","6"]:
            if spot in result:
                summary = add_result(summary, result[spot] | result["granule"])
        print(f"Added {result["granule"]["name"]}")

# write grid to map
write_map_file(grid, args.grid_file)

# write summary out to csv
write_csv_file(summary, args.summary_file)
