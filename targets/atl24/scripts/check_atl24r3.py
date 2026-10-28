import json
import argparse
import earthaccess
import pandas as pd
import geopandas as gpd
import numpy as np
from h5coro import h5coro, s3driver
from sliderule import icesat2

#
# Command Line Arguments
#
parser = argparse.ArgumentParser(description="""ATL24 Platinum Run""")
parser.add_argument('--atl03_granules', type=str,   default="data/atl24r3_validation_set.txt")
parser.add_argument('--atl03_granule',  type=str,   default=None) # "ATL03_20191215112656_12150507_006_01.h5"
args = parser.parse_args()

#
# Authenticate with Earth Access
#
print(f"Authenticating to NSIDC ...")
auth = earthaccess.login()
nsidc_creds = auth.get_s3_credentials(daac="NSIDC")

#
# Constants
#
VERSIONS = {
    "sliderule_version":    "v5.6.3",
    "atl24_plugin_version": "v3.1.2",
    "alt24_algo_version":   "eca4bf8"
}

INFO_TO_SPOT = { # [SC_ORIENT][TRACK][PAIR]
    0: {            # SC_BACKWARD
        '1': {      # TRACK
            'l': 1, # PAIR
            'r': 2,
        },
        '2': {      # TRACK
            'l': 3, # PAIR
            'r': 4,
        },
        '3': {      # TRACK
            'l': 5, # PAIR
            'r': 6,
        }
    },
    1: {            # SC_FOWARD
        '1': {      # TRACK
            'l': 6, # PAIR
            'r': 5,
        },
        '2': {      # TRACK
            'l': 4, # PAIR
            'r': 3,
        },
        '3': {      # TRACK
            'l': 2, # PAIR
            'r': 1,
        }
    }
}

ATL24_VARS = [
    "class_ph",
    "confidence",
    "delta_time",
    "ellipse_h",
    "index_ph",
    "index_seg",
    "segment_id",
    "lat_ph",
    "lon_ph",
    "night_flag",
    "ortho_h",
    "sigma_thu",
    "sigma_tvu",
    "surface_h",
    "x_atc",
    "y_atc",
    "kd",
    "surface_roughness",
    "quality_ph"
]

ATL03_PH_VARS = [
    "heights/delta_time",
    "heights/lat_ph",
    "heights/lon_ph",
    "heights/dist_ph_across",
    "heights/dist_ph_along",
    "heights/h_ph",
    "heights/quality_ph"
]

ATL03_GEO_VARS = [
    "geophys_corr/geoid",
    "geolocation/segment_dist_x",
    "geolocation/solar_elevation",
    "geolocation/ref_elev",
    "geolocation/ref_azimuth"
]

BEAMS = [
    "gt1l",
    "gt1r",
    "gt2l",
    "gt2r",
    "gt3l",
    "gt3r"
]

BEAM_TO_GT = {
    "gt1l": icesat2.GT1L,
    "gt1r": icesat2.GT1R,
    "gt2l": icesat2.GT2L,
    "gt2r": icesat2.GT2R,
    "gt3l": icesat2.GT3L,
    "gt3r": icesat2.GT3R
}

#
# Get H5 Filename
#
def get_h5_filename(atl03_granule):
    return f"sliderule-public/atl24r3/h5/{atl03_granule.replace("ATL03", "ATL24").replace(".h5", "_003_01.h5")}"

#
# Get Parquet Filename
#
def get_parquet_filename(atl03_granule):
    return f"s3://sliderule-public/atl24r3/parquet/{atl03_granule.replace("ATL03", "ATL24").replace(".h5", "_003_01.parquet")}"

#
# Read Variables in H5 File
#
def read_vars_h5(h5obj, vars):
    dfs = []
    promise = h5obj.readDatasets([f'{beam}/{var}' for beam in BEAMS for var in vars], block=False, enableAttributes=False)
    for beam in BEAMS:
        if f'{beam}/{vars[0]}' in promise:
            data = {var: promise[f'{beam}/{var}'][:] for var in vars} | {"beam": [beam for i in range(len(promise[f'{beam}/{vars[0]}']))]}
            dfs.append(gpd.pd.DataFrame(data))
    return pd.concat(dfs, ignore_index=True)

#
# Read Metadata in H5 File
#
def read_meta_h5(h5obj, meta):
    promise = h5obj.readDatasets(list(meta.values()), block=True, enableAttributes=False)
    data = {}
    for key in meta:
        try:
            data[key] = json.loads(promise[meta[key]][:])
        except:
            data[key] = promise[meta[key]][:]
    return data

#
# Read ATL24 H5 Granule
#
def read_atl24_h5(atl03_granule):
    print(f"... reading atl24 h5 granule")
    atl24_h5_file = get_h5_filename(atl03_granule)
    h5obj = h5coro.H5Coro(atl24_h5_file, s3driver.S3Driver, errorChecking=True, verbose=False, credentials={"role": True, "role":"iam"}, multiProcess=False)
    df = read_vars_h5(h5obj, ATL24_VARS)
    metadata = read_meta_h5(h5obj, {
        "sliderule": "metadata/sliderule",
        "atl24": "metadata/atl24",
        "lat_poly": "orbit_info/bounding_polygon_lat1",
        "lon_poly": "orbit_info/bounding_polygon_lon1"
    })
    h5obj.close()
    return df, metadata

#
# Read ATL24 Parquet File
#
def read_atl24_parquet(atl03_granule):
    print(f"... reading atl24 parquet file")
    atl24_parquet_file = get_parquet_filename(atl03_granule)
    return gpd.read_parquet(atl24_parquet_file)

#
# Read ATL03 H5 Granule
#
def read_atl03_h5(atl03_granule):
    print(f"... reading atl03 h5 granule")
    granule_path = f"nsidc-cumulus-prod-protected/ATLAS/ATL03/{atl03_granule[30:33]}/{atl03_granule[6:10]}/{atl03_granule[10:12]}/{atl03_granule[12:14]}/{atl03_granule}"
    h5obj = h5coro.H5Coro(granule_path, s3driver.S3Driver, errorChecking=True, verbose=False, credentials=nsidc_creds, multiProcess=False)
    ph_df = read_vars_h5(h5obj, ATL03_PH_VARS)
    geo_df = read_vars_h5(h5obj, ATL03_GEO_VARS)
    metadata = read_meta_h5(h5obj, {"sc_orient": "orbit_info/sc_orient", "lat_poly": "orbit_info/bounding_polygon_lat1", "lon_poly": "orbit_info/bounding_polygon_lon1"})
    h5obj.close()
    return ph_df, geo_df, metadata

#
# Compare Parquet to h5
#
def compare_parquet_to_h5(h5_df, parquet_df):
    print(f"... parquet to h5 check")
    # create lat and lon columns
    parquet_df["lat_ph"] = parquet_df.geometry.y
    parquet_df["lon_ph"] = parquet_df.geometry.x
    parquet_df["ortho_h"] = parquet_df["geoid_corr_h"]
    parquet_df["delta_time"] = (parquet_df.index - np.datetime64('2018-01-01T00:00:00')) / np.timedelta64(1, 's')
    parquet_df["night_flag"] = ((parquet_df["processing_flags"] & 0x20) != 0).astype(np.int8)
    # check class_ph
    allowed_classifications = [0, 1, 2, 40, 41]
    if not h5_df["class_ph"].isin(allowed_classifications).all():
        raise RuntimeError(f'Error - class_ph: unexpected classifications {set(h5_df["class_ph"]) - set(allowed_classifications)}')
    # check values (compared positionally; the two frames have different indexes)
    for var in ATL24_VARS:
        h5_vals = h5_df[var].to_numpy()
        parquet_vals = parquet_df[var].to_numpy()
        if len(h5_vals) != len(parquet_vals):
            raise RuntimeError(f'Error - {var}: mismatched length {len(h5_vals)} != {len(parquet_vals)}')
        if np.issubdtype(h5_vals.dtype, np.floating) or np.issubdtype(parquet_vals.dtype, np.floating):
            rtol = 1e-6 if np.float32 in (h5_vals.dtype, parquet_vals.dtype) else 1e-12
            diffs = ~np.isclose(h5_vals, parquet_vals, rtol=rtol, atol=0.0, equal_nan=True)
        else:
            diffs = (h5_vals != parquet_vals)
        if diffs.sum() > 0:
            max_diff = np.max(np.abs(h5_vals[diffs].astype(np.float64) - parquet_vals[diffs].astype(np.float64)))
            raise RuntimeError(f'Error - {var}: {diffs.sum()} mismatched values in {len(parquet_vals)} rows, max diff {max_diff}')

#
# Check Expectations
#
def check_expectations(parquet_df):
    print(f"... expectation check")
    vc = parquet_df["max_signal_conf"].value_counts()
    for c in [0, 1, 2, 3, 4]:
        if c not in vc:
            raise RuntimeError(f"Expected to see photons with confidence {c}")

#
# Check Metadata
#
def check_metadata(metadata):
    print(f"... metadata check")
    if metadata["atl24"]["plugin_version"] != VERSIONS["atl24_plugin_version"]:
        raise RuntimeError(f"Invalid plugin version: {metadata["atl24"]["plugin_version"]}")
    if metadata["atl24"]["sliderule_version"] != VERSIONS["sliderule_version"]:
        raise RuntimeError(f"Invalid sliderule version: {metadata["atl24"]["sliderule_version"]}")
    if metadata["atl24"]["plugin_algoinfo"] != VERSIONS["alt24_algo_version"]:
        raise RuntimeError(f"Invalid algorithm version: {metadata["atl24"]["plugin_algoinfo"]}")
    if metadata["sliderule"]["min_dem_delta"] != -100.0: # spot check
        raise RuntimeError(f"Invalid sliderule metadata")

#
# Check Bounding Polygons
#
def check_bounding_polygons(atl03_metadata, atl24_metadata):
    print(f"... bounding polygon check")
    atl03_bounding_polygon = {"lat": atl03_metadata["lat_poly"][:], "lon": atl03_metadata["lon_poly"][:]}
    atl24_bounding_polygon = {"lat": atl24_metadata["lat_poly"][:], "lon": atl24_metadata["lon_poly"][:]}
    num_coords_in_poly = len(atl03_bounding_polygon["lat"])
    if num_coords_in_poly < 4:
        raise RuntimeError(f"Mismatched values - number of coordinates in h5 bounding polygon is too small: {num_coords_in_poly}")
    elif num_coords_in_poly != len(atl24_bounding_polygon["lat"]) or num_coords_in_poly != len(atl24_bounding_polygon["lon"]):
        raise RuntimeError(f"Mismatched values - number of coordinates in h5 bounding polygon does not match ATL24 bounding polygon: {num_coords_in_poly} != {len(atl24_bounding_polygon["lat"])}, {len(atl24_bounding_polygon["lon"])}")
    for i in range(num_coords_in_poly):
        cmr_lat_delta = abs(atl03_bounding_polygon["lat"][i] - atl24_bounding_polygon["lat"][i])
        if cmr_lat_delta > 0.00000000001:
            raise RuntimeError(f"Mismatched values - h5 bounding polygon latitude {i} does not match: {atl03_bounding_polygon["lat"][i]} != {atl24_bounding_polygon["lat"][i]}")
        cmr_lon_delta = abs(atl03_bounding_polygon["lon"][i] - atl24_bounding_polygon["lon"][i])
        if cmr_lon_delta > 0.00000000001:
            raise RuntimeError(f"Mismatched values - h5 bounding polygon longitude {i} does not match: {atl03_bounding_polygon["lon"][i]} != {atl24_bounding_polygon["lon"][i]}")

#
# Check Delta Times
#
def check_delta_times(atl03_ph_df, atl24_df):
    print(f"... delta time check")
    atl03_delta_time = atl03_ph_df["heights/delta_time"].to_numpy()
    atl24_delta_time = atl24_df["delta_time"].to_numpy()
    atl24_index_ph = atl24_df["index_ph"].to_numpy()
    dt_deltas = np.abs(atl03_delta_time[atl24_index_ph] - atl24_delta_time)
    for i in np.flatnonzero(dt_deltas > 0.000001):
        raise RuntimeError(f"Mismatched values - delta_time on row {i} at photon index {atl24_index_ph[i]}: {dt_deltas[i]}")

#
# Check ATL03 Calculations
#
def check_atl03_calculations(atl03_ph_df, atl03_geo_df, atl03_metadata, atl24_df):
    print(f"... atl03 calculations check")

    # get h5 arrays
    h5_lats = atl03_ph_df["heights/lat_ph"].to_numpy()
    h5_lons = atl03_ph_df["heights/lon_ph"].to_numpy()
    h5_distacross = atl03_ph_df["heights/dist_ph_across"].to_numpy()
    h5_distalong = atl03_ph_df["heights/dist_ph_along"].to_numpy()
    h5_h = atl03_ph_df["heights/h_ph"].to_numpy()
    h5_quality_ph = atl03_ph_df["heights/quality_ph"].to_numpy()
    h5_distseg = atl03_geo_df["geolocation/segment_dist_x"].to_numpy()
    h5_geoid = atl03_geo_df["geophys_corr/geoid"].to_numpy()
    h5_solar = atl03_geo_df["geolocation/solar_elevation"].to_numpy()

    # get df series
    df_lats = atl24_df["lat_ph"].to_numpy()
    df_lons = atl24_df["lon_ph"].to_numpy()
    df_ph = atl24_df["index_ph"].to_numpy()
    df_seg = atl24_df["index_seg"].to_numpy()
    df_class = atl24_df["class_ph"].to_numpy()
    df_surface = atl24_df["surface_h"].to_numpy()
    df_y_atc = atl24_df["y_atc"].to_numpy()
    df_x_atc = atl24_df["x_atc"].to_numpy()
    df_geoid = atl24_df["ortho_h"].to_numpy()
    df_ellipse = atl24_df["ellipse_h"].to_numpy()
    df_night = atl24_df["night_flag"].to_numpy()
    df_quality_ph = atl24_df["quality_ph"].to_numpy()

    # initialize refraction stats
    lat_refraction_acc = 0.0
    lat_refraction_max = 0.0
    lon_refraction_acc = 0.0
    lon_refraction_max = 0.0
    z_refraction_acc = 0.0
    z_refraction_max = 0.0

    # check values
    for i in range(len(df_ph)):

        # x_atc
        x_delta = abs(h5_distalong[df_ph[i]] + h5_distseg[df_seg[i]] - df_x_atc[i])
        if(x_delta != 0.0):
            raise RuntimeError(f'Mismatched value - x_atc on row {i} at photon index {df_ph[i]}: {x_delta}')

        # y_atc
        y_delta = abs(h5_distacross[df_ph[i]] - df_y_atc[i])
        if(y_delta != 0.0):
            raise RuntimeError(f'Mismatched value - y_atc on row {i} at photon index {df_ph[i]}: {y_delta}')

        # geoid_corr_h
        geoid_delta = abs((h5_h[df_ph[i]] - h5_geoid[df_seg[i]]) - df_geoid[i])
        if df_geoid[i] > df_surface[i] or df_class[i] == 41:
            if(geoid_delta != 0.0):
                raise RuntimeError(f'Mismatched value - geoid on row {i} at photon index {df_ph[i]}: {geoid_delta}')
        else:
            z_refraction_acc += geoid_delta
            if geoid_delta > z_refraction_max:
                z_refraction_max = geoid_delta

        # ellipse_h
        ellipse_delta = abs(h5_h[df_ph[i]] - df_ellipse[i])
        if df_geoid[i] > df_surface[i] or df_class[i] == 41:
            if(ellipse_delta != 0.0):
                raise RuntimeError(f'Mismatched value - ellipse on row {i} at photon index {df_ph[i]}: {h5_h[df_ph[i]]} != {df_ellipse[i]} || {df_geoid[i]} {df_surface[i]} {df_class[i]}')

        # night flag
        is_night = h5_solar[df_seg[i]] < 5.0
        night_flag = df_night[i] != 0
        if is_night != night_flag:
            raise RuntimeError(f'Mismatched value - night_flag on row {i} at photon index {df_ph[i]} and segment index {df_seg[i]}: {df_night[i]} {h5_solar[df_seg[i]]}')

        # quality_ph
        if h5_quality_ph[df_ph[i]] != df_quality_ph[i]:
            raise RuntimeError(f'Mismatched value - quality_ph on row {i} at photon index {df_ph[i]}: {h5_quality_ph[df_ph[i]]} != {df_quality_ph[i]}')

        # lat_ph, lon_ph
        lat_delta = abs(h5_lats[df_ph[i]] - df_lats[i])
        lon_delta = abs(h5_lons[df_ph[i]] - df_lons[i])
        if df_geoid[i] > df_surface[i] or df_class[i] == 41:
            if(lat_delta != 0.0 or lon_delta != 0.0):
                raise RuntimeError(f'Mismatched value - lat,lon on row {i} at photon index {df_ph[i]}: {lat_delta}, {lon_delta}, {df_geoid[i] - df_surface[i]}, {df_class[i]}')
        else:
            lat_refraction_acc += lat_delta
            if lat_delta > lat_refraction_max:
                lat_refraction_max = lat_delta
            lon_refraction_acc += lon_delta
            if lon_delta > lon_refraction_max:
                lon_refraction_max = lon_delta

    print(f'... refraction avg {lat_refraction_acc / len(h5_lats):.2e}, {lon_refraction_acc / len(h5_lons):.2e}, {z_refraction_acc / len(h5_h):.2e}')
    print(f'... refraction max {lat_refraction_max:.2e}, {lon_refraction_max:.2e}, {z_refraction_max:.2e}')

#
# Analyze ATL24 Result
#
def analyze_atl24_result(atl03_granule):
    try:
        print(f"Checking {atl03_granule} results ...")
        atl03_ph_df_all, atl03_geo_df_all, atl03_metadata = read_atl03_h5(atl03_granule)
        atl24_df_all, atl24_metadata = read_atl24_h5(atl03_granule)
        parquet_df_all = read_atl24_parquet(atl03_granule)
        try:
            check_expectations(parquet_df_all)
        except Exception as e:
            print(f"??? {atl03_granule} - warning: {e}")
        check_metadata(atl24_metadata)
        check_bounding_polygons(atl03_metadata, atl24_metadata)
        for beam in BEAMS:
            print(f"--- checking beam {beam}")
            atl03_ph_df = atl03_ph_df_all[atl03_ph_df_all["beam"] == beam]
            atl03_geo_df = atl03_geo_df_all[atl03_geo_df_all["beam"] == beam]
            atl24_df = atl24_df_all[atl24_df_all["beam"] == beam]
            parquet_df = parquet_df_all[parquet_df_all["gt"] == BEAM_TO_GT[beam]] # .reset_index(drop=True)
            compare_parquet_to_h5(atl24_df, parquet_df)
            check_delta_times(atl03_ph_df, atl24_df)
            check_atl03_calculations(atl03_ph_df, atl03_geo_df, atl03_metadata, atl24_df)
        print(f">>> {atl03_granule} - successfully checked {len(parquet_df_all)} rows")
    except Exception as e:
        print(f"!!! {atl03_granule} - exception: {e}")

#
# Main
#
if __name__ == "__main__":
    if args.atl03_granule:
        atl03_granules = [args.atl03_granule]
    else:
        print(f"Reading granules from {args.atl03_granules} ...")
        with open(args.atl03_granules, "r") as file:
            atl03_granules = [line.strip() for line in file.readlines() if line.strip()]
    for atl03_granule in atl03_granules:
        analyze_atl24_result(atl03_granule)
