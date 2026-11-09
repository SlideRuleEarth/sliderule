import os
import json
import netrc
import h5py
import duckdb
import traceback
import boto3
import requests
import numpy as np
import geopandas as gpd
from urllib.parse import urlparse
from sliderule import pysr

# runner
runner = pysr.Runner()
granule, result_file = runner.arguments

# constants
OUTPUT_BUCKET = "sliderule-public"
OUTPUT_PREFIX = "atl06tv4"
TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally
NSIDC_BUCKET = "nsidc-cumulus-prod-protected"
NSIDC_S3_CREDENTIALS = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
ATLAS_SDP_EPOCH = np.datetime64("2018-01-01T00:00:00", "ns") # delta_time is seconds since this epoch; no leap seconds have occurred since
BEAMS = {"gt1l": 10, "gt1r": 20, "gt2l": 30, "gt2r": 40, "gt3l": 50, "gt3r": 60} # integer ground track used by sliderule
SOURCE_COLUMNS = [
    "atl06_quality_summary",
    "delta_time",
    "fpb_warning_flag",
    "h_li",
    "h_li_sigma",
    "latitude",
    "longitude",
    "segment_id",
    "sigma_geo_h",
    "bias_correction/fpb_mean_corr",
    "bias_correction/fpb_mean_corr_sigma",
    "bias_correction/fpb_med_corr",
    "bias_correction/fpb_med_corr_sigma",
    "bias_correction/fpb_n_corr",
    "bias_correction/med_r_fit",
    "bias_correction/tx_mean_corr",
    "bias_correction/tx_med_corr",
    "dem/dem_flag",
    "dem/dem_h",
    "dem/geoid_free2mean",
    "dem/geoid_h",
    "fit_statistics/dh_fit_dx",
    "fit_statistics/dh_fit_dx_sigma",
    "fit_statistics/dh_fit_dy",
    "fit_statistics/h_expected_rms",
    "fit_statistics/h_mean",
    "fit_statistics/h_rms_misfit",
    "fit_statistics/h_robust_sprd",
    "fit_statistics/n_fit_photons",
    "fit_statistics/n_seg_pulses",
    "fit_statistics/sigma_h_mean",
    "fit_statistics/signal_selection_source",
    "fit_statistics/signal_selection_source_status",
    "fit_statistics/snr",
    "fit_statistics/snr_significance",
    "fit_statistics/w_surface_window_final",
    "geophysical/bckgrd",
    "geophysical/bsnow_conf",
    "geophysical/bsnow_h",
    "geophysical/bsnow_od",
    "geophysical/cloud_flg_asr",
    "geophysical/cloud_flg_atm",
    "geophysical/dac",
    "geophysical/e_bckgrd",
    "geophysical/layer_flag",
    "geophysical/msw_flag",
    "geophysical/neutat_delay_total",
    "geophysical/r_eff",
    "geophysical/solar_azimuth",
    "geophysical/solar_elevation",
    "geophysical/tide_earth",
    "geophysical/tide_earth_free2mean",
    "geophysical/tide_equilibrium",
    "geophysical/tide_load",
    "geophysical/tide_ocean",
    "geophysical/tide_pole",
    "ground_track/ref_azimuth",
    "ground_track/ref_coelv",
    "ground_track/seg_azimuth",
    "ground_track/sigma_geo_at",
    "ground_track/sigma_geo_r",
    "ground_track/sigma_geo_xt",
    "ground_track/x_atc",
    "ground_track/y_atc",
]

# create S3 client
s3 = boto3.client("s3")

# keeps basic auth on the redirect to earthdata login, which requests otherwise strips when the host changes
class EarthdataSession(requests.Session):
    def rebuild_auth(self, prepared_request, response):
        if urlparse(prepared_request.url).hostname != "urs.earthdata.nasa.gov":
            prepared_request.headers.pop("Authorization", None)

# get temporary s3 credentials for the nsidc bucket
def nsidc_credentials():
    username, password = runner.secrets.get("earthdata_username"), runner.secrets.get("earthdata_password")
    if not username or not password:
        # local runs have no secrets file, so fall back to ~/.netrc
        username, _, password = netrc.netrc().authenticators("urs.earthdata.nasa.gov")
    with EarthdataSession() as session:
        session.auth = (username, password)
        response = session.get(NSIDC_S3_CREDENTIALS)
        response.raise_for_status()
        return json.loads(response.text) # served as text/html

# build geodataframe
def build_gdf(granule):
    # download h5 file to /tmp directory
    credentials = nsidc_credentials()
    nsidc = boto3.client("s3", region_name="us-west-2",
                         aws_access_key_id=credentials["accessKeyId"],
                         aws_secret_access_key=credentials["secretAccessKey"],
                         aws_session_token=credentials["sessionToken"])
    version = granule[30:33] # ATL06_YYYYMMDDHHMMSS_RRRRCCSS_VVV_RR.h5
    key = f"ATLAS/ATL06/{version}/{granule[6:10]}/{granule[10:12]}/{granule[12:14]}/{granule}"
    local_h5_file = f"/tmp/{granule}"
    nsidc.download_file(NSIDC_BUCKET, key, local_h5_file)
    # read source columns from each ground track
    frames = []
    try:
        with h5py.File(local_h5_file, "r") as h5:
            for beam, gt in BEAMS.items():
                segments = h5.get(f"{beam}/land_ice_segments")
                if segments is None: # beams without data are left out of the granule
                    continue
                df = gpd.pd.DataFrame({column.split("/")[-1]: segments[column][:] for column in SOURCE_COLUMNS})
                df["gt"] = np.uint8(gt)
                frames.append(df)
    finally:
        os.remove(local_h5_file)
    if not frames:
        return None
    # create GeoDataFrame
    df = gpd.pd.concat(frames, ignore_index=True)
    df.insert(0, "time_ns", ATLAS_SDP_EPOCH + (df["delta_time"].to_numpy() * 1e9).astype("timedelta64[ns]"))
    crs = "EPSG:7912" if int(version) <= 6 else "EPSG:9989" # ITRF2014 through release 006, ITRF2020 after, as sliderule does
    return gpd.GeoDataFrame(df, geometry=gpd.points_from_xy(df["longitude"], df["latitude"]), crs=crs)

# assign cell IDs
def assign_cell_ids(gdf, level):
    connection = duckdb.connect()
    connection.sql("LOAD geography;")
    geometry = gdf.geometry.to_crs("EPSG:4326") # S2 is defined on geographic lon/lat, so reproject to WGS84 first
    coords = gpd.pd.DataFrame({"lon": geometry.x.to_numpy(), "lat": geometry.y.to_numpy()})
    connection.register("coords", coords)
    return connection.sql(f"SELECT s2_cell_parent(s2_cellfromlonlat(lon, lat), {level})::UBIGINT AS cell_id FROM coords").fetchnumpy()["cell_id"]

# ##############
# Main
# ##############

try:

    # setup duckdb
    duckdb.connect().sql("INSTALL geography FROM community;")

    # read granule into a geodataframe
    gdf = build_gdf(granule)
    if gdf is None or len(gdf) == 0:
        raise RuntimeError("no data")

    # assign cell ids
    gdf["cell_id"] = assign_cell_ids(gdf, TILE_S2_CELL_LEVEL)
    gdf = gdf.reset_index().set_index("cell_id").sort_index(kind="stable")
    cell_ids = gdf.index.unique()

    # write partitions for each cell id
    for cell_id in cell_ids:
        l7_gdf = gdf[gdf.index == cell_id]
        local_parquet_file = f"/tmp/{granule.replace('.h5', '')}.S{cell_id:016X}.parquet"
        remote_parquet_file = f"{OUTPUT_PREFIX}/partitions/S{cell_id:016X}/{granule.replace('.h5', '.parquet')}"
        l7_gdf.to_parquet(local_parquet_file, index=False)
        s3.upload_file(local_parquet_file, OUTPUT_BUCKET, remote_parquet_file)
        runner.output(remote_parquet_file)
        runner.message(f"{len(gdf)} rows in {len(cell_ids)} partitions")
        os.remove(local_parquet_file)

except RuntimeError as e:

    runner.fail(f"Exception: {e}")

except Exception as e:

    runner.fail(f"Unhandled exception: {traceback.format_exc()}")

# finish
runner.finish()