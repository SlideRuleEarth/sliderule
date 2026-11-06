import os
import sys
import time
import json
import tempfile
import traceback
from urllib.parse import urlparse
import boto3
from botocore import UNSIGNED
from botocore.config import Config
import rasterio
from rasterio.session import AWSSession
from rasterio.warp import transform as warp_transform
import numpy as np
import pandas as pd
import geopandas as gpd

# arguments
granule = sys.argv[1]
result_file = sys.argv[2]

# globals
s3 = boto3.client("s3")

# convert to json-native types with NaN/NaT as None; json's default hook can't do this since np.float64 subclasses float
def json_safe(o):
    if isinstance(o, dict):
        return {k: json_safe(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [json_safe(v) for v in o]
    if isinstance(o, np.ndarray):
        return json_safe(o.tolist())
    if pd.api.types.is_scalar(o) and pd.isna(o):
        return None
    if isinstance(o, pd.Timestamp):
        return o.isoformat()
    if isinstance(o, np.generic): # np.int64, np.float32, np.bool_, ...
        return o.item()
    return o

# parse URL into bucket and subfolder
def parse_url(url):
    path = url.split("s3://")[-1]
    bucket = path.split("/")[0]
    subfolder = '/'.join(path.split("/")[1:])
    return bucket, subfolder

# calculate a mean per bin
def bin_mean(values):
    values = np.asarray(values, dtype=np.float64)
    valid = ~np.isnan(values)
    sums = np.add.reduceat(np.where(valid, values, 0.0), bin_starts)
    counts = np.add.reduceat(valid.astype(np.int64), bin_starts)
    with np.errstate(invalid="ignore", divide="ignore"):
        return np.where(counts > 0, sums / counts, np.nan)

# bluetopo dataset
BLUETOPO_BUCKET = "noaa-ocs-nationalbathymetry-pds"
BLUETOPO_PREFIX = "BlueTopo/"
BLUETOPO_INDEX = "_BlueTopo_Tile_Scheme"
BLUETOPO_ENDPOINT = "s3.us-east-1.amazonaws.com"
BLUETOPO_REGION = "us-east-1"

# load the most recent bluetopo tile scheme (only tiles that have a geotiff)
def load_bluetopo_tiles():
    s3_unsigned = boto3.client("s3", region_name=BLUETOPO_REGION, endpoint_url=f"https://{BLUETOPO_ENDPOINT}", config=Config(signature_version=UNSIGNED))
    paginator = s3_unsigned.get_paginator("list_objects_v2")
    keys = [obj["Key"] for page in paginator.paginate(Bucket=BLUETOPO_BUCKET, Prefix=f"{BLUETOPO_PREFIX}{BLUETOPO_INDEX}/") for obj in page.get("Contents", []) if obj["Key"].endswith(".gpkg")]
    if len(keys) == 0:
        raise RuntimeError("Unable to find BlueTopo tile scheme")
    with tempfile.TemporaryDirectory() as tmpdir:
        index_file = os.path.join(tmpdir, "tile_scheme.gpkg")
        s3_unsigned.download_file(BLUETOPO_BUCKET, sorted(keys)[-1], index_file) # file names are timestamped
        tiles = gpd.read_file(index_file, columns=["tile", "GeoTIFF_Link"])
    tiles = tiles[tiles["GeoTIFF_Link"].notna() & (tiles["GeoTIFF_Link"] != "")]
    return tiles.to_crs("EPSG:4326")

# sample bluetopo elevation and uncertainty bands at each longitude/latitude
def sample_bluetopo(longitude, latitude, tiles):
    elevation = np.full(len(longitude), np.nan)
    uncertainty = np.full(len(longitude), np.nan)
    points = gpd.GeoDataFrame({"idx": np.arange(len(longitude))}, geometry=gpd.points_from_xy(longitude, latitude), crs="EPSG:4326")
    points = points[np.isfinite(longitude) & np.isfinite(latitude)]
    hits = gpd.sjoin(points, tiles, how="inner", predicate="intersects").drop_duplicates(subset="idx") # points on shared tile edges match twice
    session = AWSSession(aws_unsigned=True, endpoint_url=BLUETOPO_ENDPOINT, region_name=BLUETOPO_REGION)
    with rasterio.Env(session=session, GDAL_DISABLE_READDIR_ON_OPEN="EMPTY_DIR"):
        for link, group in hits.groupby("GeoTIFF_Link"):
            idx = group["idx"].to_numpy()
            with rasterio.open(f"s3://{BLUETOPO_BUCKET}/{urlparse(link).path.lstrip('/')}") as src:
                xs, ys = warp_transform("EPSG:4326", src.crs, longitude[idx], latitude[idx])
                band = {name: i + 1 for i, name in enumerate(src.descriptions)}
                values = np.array(list(src.sample(zip(xs, ys), indexes=[band["Elevation"], band["Uncertainty"]], masked=True)))
                values = np.ma.filled(np.ma.asarray(values).astype(np.float64), np.nan)
                elevation[idx] = values[:, 0]
                uncertainty[idx] = values[:, 1]
    return elevation, uncertainty

# initialize result
input_file = f"s3://sliderule-public/atl24r3/parquet/{granule}"
output_file = f"s3://sliderule-public/atl24r3/bluetopo/{granule.replace('.parquet', '_bluetopo.parquet')}"
result = {
    "status": True,
    "build": "local",
    "start": time.time(),
    "outputs": [],
    "messages": []
}

try:
    # read granule into GeoDataFrames
    result["messages"].append(f"Processing {granule}")
    gdf = gpd.read_parquet(input_file, columns=[
        "geometry",
        "class_ph",
        "x_atc",
        "geoid_corr_h",
        "ellipse_h",
        "surface_h",
        "surface_roughness",
        "kd",
        "confidence",
        "sigma_tvu",
        "processing_flags",
        "max_signal_conf"
    ])

    # add depth column to gdf
    gdf["depth"] = gdf["surface_h"] - gdf["geoid_corr_h"]

    # create 10 meter along track bins
    bin_size = 10.0
    x_atc = gdf["x_atc"].to_numpy()
    bin_id = np.floor(x_atc / bin_size).astype(np.int64)
    bin_starts = np.flatnonzero(np.r_[True, bin_id[1:] != bin_id[:-1]]) if len(bin_id) > 0 else np.array([], dtype=np.int64)
    if len(bin_starts) == 0:
        raise RuntimeError("Dataframe did not include any bins")

    # calculate per-bin statistics; each bin is [bin_starts[i], bin_starts[i+1])
    bins = pd.DataFrame({
        "x_atc": bin_id[bin_starts] * bin_size,
        "latitude": bin_mean(gdf.geometry.y.to_numpy()),
        "longitude": bin_mean(gdf.geometry.x.to_numpy()),
        "depth": bin_mean(gdf["depth"].to_numpy()),
        "non_noise": np.logical_or.reduceat(gdf["class_ph"].to_numpy() != 0, bin_starts),
        "max_confidence": np.fmax.reduceat(gdf["confidence"].to_numpy(dtype=np.float64), bin_starts),
        "night": bin_mean((gdf["processing_flags"].to_numpy().astype(np.int64) & 0x20) != 0),
        "kd": bin_mean(gdf["kd"].to_numpy()),
        "surface_roughness": bin_mean(gdf["surface_roughness"].to_numpy()),
    })

    # sample bluetopo at each bin; depth is positive down (bluetopo elevation is negative below the vertical datum)
    bluetopo_tiles = load_bluetopo_tiles()
    bluetopo_elevation, bluetopo_uncertainty = sample_bluetopo(bins["longitude"].to_numpy(), bins["latitude"].to_numpy(), bluetopo_tiles)
    bins["bluetopo_depth"] = -bluetopo_elevation
    bins["bluetopo_uncertainty"] = bluetopo_uncertainty
    result["messages"].append(f"Sampled BlueTopo for {np.count_nonzero(~np.isnan(bluetopo_elevation))} of {len(bins)} bins")

    # write outputs
    result["outputs"].append(output_file)

except Exception:

    # errors
    result["status"] = False
    result["messages"].append(f"Unhandled exception: {traceback.format_exc()}")

# finish
result["stop"] = time.time()
with open(result_file, "w") as file:
    json.dump(json_safe(result), file, allow_nan=False)