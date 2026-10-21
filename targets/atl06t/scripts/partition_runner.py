import os
import sys
import json
import time
import duckdb
import sliderule
import traceback
import boto3
import geopandas as gpd

# arguments
granule = sys.argv[1]
result_file = sys.argv[2]

# constants
TILE_S2_CELL_LEVEL = 7 # ~5188 km^2, 98K cells globally
OUTPUT_BUCKET = "sliderule-public"
OUTPUT_PREFIX = "atl06tv2"

# create S3 client
s3 = boto3.client("s3")

# create sliderule session
session = sliderule.create_session(domain="localhost", cluster=None)

# build geodataframe
def build_gdf(granule):
    return sliderule.run("atl06x", {
        "atl06_fields": [
            "fpb_warning_flag",
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
            "fit_statistics/dh_fit_dx_sigma",
            "fit_statistics/dh_fit_dy",
            "fit_statistics/h_expected_rms",
            "fit_statistics/h_mean",
            "fit_statistics/h_rms_misfit",
            "fit_statistics/n_seg_pulses",
            "fit_statistics/sigma_h_mean",
#            "fit_statistics/signal_selection_source",
#            "fit_statistics/signal_selection_source_status",
            "fit_statistics/snr",
            "fit_statistics/snr_significance",
            "geophysical/bckgrd",
            "geophysical/bsnow_od",
            "geophysical/cloud_flg_asr",
            "geophysical/cloud_flg_atm",
            "geophysical/dac",
            "geophysical/e_bckgrd",
            "geophysical/layer_flag",
            "geophysical/msw_flag",
            "geophysical/neutat_delay_total",
            "geophysical/solar_azimuth",
            "geophysical/solar_elevation",
            "geophysical/tide_earth",
            "geophysical/tide_earth_free2mean",
            "geophysical/tide_equilibrium",
            "geophysical/tide_load",
            "geophysical/tide_pole",
            "ground_track/ref_azimuth",
            "ground_track/ref_coelv",
            "ground_track/sigma_geo_at",
            "ground_track/sigma_geo_r",
            "ground_track/sigma_geo_xt"
        ]
    }, resources=[granule], session=session)

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

result = {
    "status": True,
    "build": "local",
    "start": time.time(),
    "outputs": [],
    "messages": []
}

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
        result["outputs"].append(remote_parquet_file)
        result["messages"].append(f"{len(gdf)} rows in {len(cell_ids)} partitions")
        os.remove(local_parquet_file)

except RuntimeError as e:

    # errors
    result["status"] = False
    result["messages"].append(f"Exception: {e.what()}")

except Exception as e:

    # errors
    result["status"] = False
    result["messages"].append(f"Unhandled exception: {traceback.format_exc()}")


# finish
result["stop"] = time.time()
with open(result_file, "w") as file:
    json.dump(result, file)