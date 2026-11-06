import os
import re
import sys
import time
import json
import zipfile
import tempfile
import traceback
import urllib.request
from datetime import datetime, timezone
from urllib.parse import urlparse
import boto3
import pyproj
from botocore import UNSIGNED
from botocore.config import Config
import rasterio
from rasterio.session import AWSSession
from rasterio.warp import transform as warp_transform
import numpy as np
import pandas as pd
import geopandas as gpd
from sliderule import pysr

# runner
runner = pysr.Runner()

# arguments
granule, result_file = runner.arguments

# globals
s3 = boto3.client("s3")

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

# map each photon to its bin index
def photon_bins():
    return np.repeat(np.arange(len(bin_starts)), np.diff(np.r_[bin_starts, len(bin_id)]))

# count photons per bin in each of num_categories categories; categories outside [0, num_categories) are not counted
def bin_counts(categories, num_categories):
    categories = np.asarray(categories)
    valid = (categories >= 0) & (categories < num_categories)
    flat = photon_bins()[valid] * num_categories + categories[valid].astype(np.int64)
    counts = np.bincount(flat, minlength=len(bin_starts) * num_categories).reshape(len(bin_starts), num_categories)
    return list(counts)

# max_signal_conf values counted per bin
MAX_SIGNAL_CONF_VALUES = np.array([-2, -1, 0, 1, 2, 3, 4])

# edges of the photon depth minus bluetopo depth histogram: (<-15), [-15,-7), [-7,-3), [-3,-1), [-1,0), [0,1), [1,3), [3,7), [7,15), (>=15)
BLUETOPO_DELTA_EDGES = np.array([-15.0, -7.0, -3.0, -1.0, 0.0, 1.0, 3.0, 7.0, 15.0])

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

# geoid grids used by bluetopo to realize its vertical datum (from the VERTICALDATUMWKT tag of each tile);
# tiles without the tag are assumed to use GEOID12B like the rest of the CONUS/PRVI tiles
GEOID_GRIDS = {
    "g2012bu0": "us_noaa_g2012bu0.tif",     # GEOID12B CONUS (PROJ CDN)
    "g2012bp0": "us_noaa_g2012bp0.tif",     # GEOID12B PR/VI (PROJ CDN)
    "AK_17B": None,                         # xGEOID17B Alaska (VDatum, downloaded on demand)
}
GEOID_UNKNOWN = "unknown"
GEOID_FALLBACK = ["g2012bu0", "g2012bp0"]
XGEOID17B_URL = "https://vdatum.noaa.gov/download/data/vdatum_XGEOID17B.zip"
XGEOID17B_MEMBER = "vdatum/core/xgeoid17b/AK_17B.gtx"

# get the geoid grid name bluetopo used for a tile
def bluetopo_geoid(src):
    match = re.search(r"(" + "|".join(GEOID_GRIDS.keys()) + r")\.gtx", src.tags().get("VERTICALDATUMWKT", ""))
    return match.group(1) if match else GEOID_UNKNOWN

# path or PROJ CDN name of a geoid grid
def geoid_grid_file(name, workdir):
    if GEOID_GRIDS[name] is not None:
        return GEOID_GRIDS[name]
    grid_file = os.path.join(workdir, os.path.basename(XGEOID17B_MEMBER))
    if not os.path.exists(grid_file):
        zip_file = os.path.join(workdir, "xgeoid.zip")
        urllib.request.urlretrieve(XGEOID17B_URL, zip_file)
        with zipfile.ZipFile(zip_file) as z, z.open(XGEOID17B_MEMBER) as gtx, open(grid_file, "wb") as out:
            out.write(gtx.read())
        os.remove(zip_file)
    return grid_file

# geoid undulation (NAD83(2011) ellipsoid height of the vertical datum surface); inf outside the grid
def geoid_undulation(name, longitude, latitude, workdir):
    pipeline = ("+proj=pipeline +step +proj=unitconvert +xy_in=deg +xy_out=rad "
                f"+step +proj=vgridshift +grids={geoid_grid_file(name, workdir)} +multiplier=1 "
                "+step +proj=unitconvert +xy_in=rad +xy_out=deg")
    _, _, undulation = pyproj.Transformer.from_pipeline(pipeline).transform(longitude, latitude, np.zeros(len(longitude)), errcheck=False)
    return np.where(np.isfinite(undulation), undulation, np.nan)

# correction to add to bluetopo depth to move it from the bluetopo vertical datum to the ATL24 orthometric heights:
#   H_atl24 = H_bluetopo + N_bluetopo + (h_itrf2014 - h_nad83) - N_atl24, and depth = -H
def bluetopo_depth_correction(longitude, latitude, geoids, atl24_geoid, epoch):
    pyproj.network.set_network_enabled(True)
    longitude, latitude = np.asarray(longitude, dtype=np.float64), np.asarray(latitude, dtype=np.float64)
    undulation = np.full(len(longitude), np.nan)
    with tempfile.TemporaryDirectory() as workdir:
        for name in set(g for g in geoids if g is not None):
            idx = np.flatnonzero(np.array([g == name for g in geoids]))
            if name == GEOID_UNKNOWN:
                for fallback in GEOID_FALLBACK:
                    missing = idx[np.isnan(undulation[idx])]
                    undulation[missing] = geoid_undulation(fallback, longitude[missing], latitude[missing], workdir)
            else:
                undulation[idx] = geoid_undulation(name, longitude[idx], latitude[idx], workdir)
    nad83_to_itrf = pyproj.Transformer.from_crs("EPSG:6319", "EPSG:7912", always_xy=True, only_best=True) # NAD83(2011) -> ITRF2014, time dependent
    _, _, frame_shift, _ = nad83_to_itrf.transform(longitude, latitude, np.zeros(len(longitude)), np.full(len(longitude), epoch), errcheck=False)
    return atl24_geoid - undulation - np.where(np.isfinite(frame_shift), frame_shift, np.nan)

# decimal year of the granule start time (ATL24_YYYYMMDDhhmmss_...)
def granule_epoch(name):
    start = datetime.strptime(name.split("_")[1], "%Y%m%d%H%M%S").replace(tzinfo=timezone.utc)
    year_start = datetime(start.year, 1, 1, tzinfo=timezone.utc)
    year_end = datetime(start.year + 1, 1, 1, tzinfo=timezone.utc)
    return start.year + (start - year_start).total_seconds() / (year_end - year_start).total_seconds()

# sample bluetopo elevation and uncertainty bands at each longitude/latitude
def sample_bluetopo(longitude, latitude, tiles):
    elevation = np.full(len(longitude), np.nan)
    uncertainty = np.full(len(longitude), np.nan)
    geoids = np.full(len(longitude), None, dtype=object)
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
                geoids[idx] = bluetopo_geoid(src)
    return elevation, uncertainty, geoids

# initialize result
input_file = f"s3://sliderule-public/atl24r3/parquet/{granule}"
output_file = f"s3://sliderule-public/atl24r3/bluetopo/{granule.replace('.parquet', '_bluetopo.parquet')}"

try:
    # read granule into GeoDataFrames
    runner.message(f"Reading {granule}")
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

    # orthometric height of bathymetry photons only (NaN otherwise so they are excluded from bin means)
    bathy_ortho_h = np.where(gdf["class_ph"].to_numpy() == 40, gdf["geoid_corr_h"].to_numpy(dtype=np.float64), np.nan)

    # create 10 meter along track bins
    bin_size = 10.0
    x_atc = gdf["x_atc"].to_numpy()
    bin_id = np.floor(x_atc / bin_size).astype(np.int64)
    bin_starts = np.flatnonzero(np.r_[True, bin_id[1:] != bin_id[:-1]]) if len(bin_id) > 0 else np.array([], dtype=np.int64)
    if len(bin_starts) == 0:
        raise RuntimeError("Dataframe did not include any bins")

    # calculate per-bin statistics; each bin is [bin_starts[i], bin_starts[i+1])
    runner.message(f"Binning {len(gdf)} rows of {granule}")
    bins = pd.DataFrame({
        "x_atc": bin_id[bin_starts] * bin_size,
        "latitude": bin_mean(gdf.geometry.y.to_numpy()),
        "longitude": bin_mean(gdf.geometry.x.to_numpy()),
        "ortho_h": bin_mean(bathy_ortho_h),
        "non_noise": np.logical_or.reduceat(gdf["class_ph"].to_numpy() != 0, bin_starts),
        "max_confidence": np.fmax.reduceat(gdf["confidence"].to_numpy(dtype=np.float64), bin_starts),
        "night": bin_mean((gdf["processing_flags"].to_numpy().astype(np.int64) & 0x20) != 0),
        "kd": bin_mean(gdf["kd"].to_numpy()),
        "surface_roughness": bin_mean(gdf["surface_roughness"].to_numpy()),
    })

    # counts of photons (all classes) per max_signal_conf value
    runner.message(f"Counting max_signal_conf for {len(bins)} bins in {granule}")
    max_signal_conf = gdf["max_signal_conf"].to_numpy(dtype=np.float64)
    max_signal_conf_index = np.where(np.isin(max_signal_conf, MAX_SIGNAL_CONF_VALUES), max_signal_conf - MAX_SIGNAL_CONF_VALUES[0], -1)
    bins["max_signal_conf_counts"] = bin_counts(max_signal_conf_index, len(MAX_SIGNAL_CONF_VALUES))

    # sample bluetopo at each bin; depth is positive down (bluetopo elevation is negative below the vertical datum)
    runner.message(f"Sampling BlueTopo for {len(bins)} bins in {granule}")
    bluetopo_tiles = load_bluetopo_tiles()
    bluetopo_elevation, bluetopo_uncertainty, bluetopo_geoids = sample_bluetopo(bins["longitude"].to_numpy(), bins["latitude"].to_numpy(), bluetopo_tiles)
    bins["bluetopo_depth"] = -bluetopo_elevation
    bins["bluetopo_uncertainty"] = bluetopo_uncertainty
    bins["bluetopo_geoid"] = bluetopo_geoids
    runner.message(f"Sampled BlueTopo for {np.count_nonzero(~np.isnan(bluetopo_elevation))} of {len(bins)} bins")

    # vertical datum correction; bluetopo_depth + bluetopo_correction is depth relative to the ATL24 (EGM2008) geoid
    runner.message(f"Generating vertical datum correction for {granule}")
    atl24_geoid = bin_mean(gdf["ellipse_h"].to_numpy() - gdf["geoid_corr_h"].to_numpy())
    bins["bluetopo_correction"] = bluetopo_depth_correction(bins["longitude"].to_numpy(), bins["latitude"].to_numpy(), bluetopo_geoids, atl24_geoid, granule_epoch(granule))

    # calculate a delta between bluetopo and the atl24 bathymetry in the bin
    runner.message(f"Calculating BlueTopo deltas for {len(bins)} bins in {granule}")
    bins["bluetopo_residual"] = bins["ortho_h"] + bins["bluetopo_depth"] + bins["bluetopo_correction"]

    # histogram of photon depth (all classes) minus corrected bluetopo depth, both relative to the ATL24 (EGM2008) geoid
    corrected_bluetopo_depth = (bins["bluetopo_depth"] + bins["bluetopo_correction"]).to_numpy()
    bluetopo_delta = -gdf["geoid_corr_h"].to_numpy(dtype=np.float64) - corrected_bluetopo_depth[photon_bins()]
    bluetopo_delta_index = np.where(np.isfinite(bluetopo_delta), np.searchsorted(BLUETOPO_DELTA_EDGES, bluetopo_delta, side="right"), -1)
    bins["bluetopo_delta_hist"] = bin_counts(bluetopo_delta_index, len(BLUETOPO_DELTA_EDGES) + 1)

    # write outputs
    runner.output(output_file)

except Exception:

    # errors
    runner.fail()
    runner.message(f"Unhandled exception: {traceback.format_exc()}")

# finish
runner.finish()
