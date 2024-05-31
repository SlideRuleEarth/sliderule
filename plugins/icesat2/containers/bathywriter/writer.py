# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import geopandas as gpd
import pandas as pd
import numpy as np
import h5py
import sys
import json

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

# constants
ATLAS_GPS_EPOCH = 1198800018

# check command line parameters
if len(sys.argv) <= 1:
    print("Not enough parameters: python writer.py <control json file>")
    sys.exit()

# read control json
control_json = sys.argv[1]
with open(control_json, 'r') as json_file:
    control = json.load(json_file)

# get inputs from control
input_files = control["input_files"]
output_parms = control["output_parms"]
atl24_filename = control["atl24_filename"]

# read in data
spot_table = {}
# read in photons as dataframe from csv file
for spot in input_files["spot_photons"]:
    if spot not in spot_table:
        spot_table[spot] = {}
    spot_table[spot]["df"] = pd.read_csv(input_files["spot_photons"][spot])
print("Read photon data into dataframes")
# read in granule info as dictionary from json file and add to dataframes
for spot in input_files["spot_granule"]:
    spot_table[spot]["info"] = json.load(open(input_files["spot_granule"][spot], 'r'))
    for key in spot_table[spot]["info"]:
        spot_table[spot]["df"][key] = spot_table[spot]["info"][key]
print("Added metadata columns to dataframes")
# read in photon classifications as dataframe from csv file and merge into dataframes
for classifier in input_files["classifiers"]:
    for spot in input_files["classifiers"][classifier]:
        classifier_df = pd.read_csv(input_files["classifiers"][classifier][spot])
        classifier_df.rename(columns={'class_ph': classifier}, inplace=True)
        spot_table[spot]["df"] = pd.merge(spot_table[spot]["df"], classifier_df, on='index_ph', how='left')
print("Added photon classifications to dataframes")

# get granule level info from first json file in the table of spot granule files
# the data being pulled out of the file is (should be) consistent across all the granule files
first_key = list(spot_table.keys())[0]
representative_info = spot_table[first_key]["info"]
granule_info = {
    "rgt": representative_info["rgt"],
    "region": representative_info["region"],
    "cycle": representative_info["cycle"]
}

# Non-HDF5 Outputs
if output_parms["format"] != "hdf5":

    # concatenate (vertically) all dataframes
    spot_dfs = [spot_table[spot]["df"] for spot in spot_table]
    df = pd.concat(spot_dfs)
    print("Concatenated data frames into a single data frame")

    # write output
    if output_parms["format"] == "csv":
        df.to_csv(atl24_filename, index=False)
        print("CSV file written: " + atl24_filename)
    elif output_parms["format"] == "parquet" or output_parms["format"] == "geoparquet":
        if output_parms["as_geo"] or output_parms["format"] == "geoparquet":
            df['time'] = df['time'].astype('datetime64[ns]')
            geometry = gpd.points_from_xy('longitude', 'latitude', 'geoid_corr_h')
            df.drop(columns=['longitude', 'latitude'], inplace=True)
            gdf = gpd.GeoDataFrame(df, geometry=geometry, crs="EPSG:7912")
            gdf.set_index('time', inplace=True)
            gdf.sort_index(inplace=True)
            gdf.to_parquet(atl24_filename, index=None)
            print("GeoParquet file written: " + atl24_filename)
        else:
            df.to_parquet(atl24_filename, index=None)
            print("Parquet file written: " + atl24_filename)

# HDF5 Output
else:

    with h5py.File(atl24_filename, 'w') as hf:

        # ancillary data
        ancillary_group = hf.create_group("ancillary_data")
        ancillary_group.create_dataset("atlas_sdp_gps_epoch", data=ATLAS_GPS_EPOCH)

        # spots
        for spot in spot_table:
            spot_info = spot_table[spot]["info"]
            spot_df = spot_table[spot]["df"]
            beam_group = hf.create_group(spot_info["beam"]) # e.g. gt1r, gt2l, etc.
            beam_group.create_dataset("index_ph", data=spot_df["index_ph"])            
            spot_df["delta_time"] = (spot_df["time"] / 1000000000.0) - ATLAS_GPS_EPOCH
            beam_group.create_dataset("delta_time", data=spot_df["delta_time"])
            beam_group.create_dataset("x_atc_corr", data=spot_df["x_atc"])
            beam_group.create_dataset("y_atc_corr", data=spot_df["y_atc"])
            beam_group.create_dataset("lat_corr", data=spot_df["latitude"])
            beam_group.create_dataset("lon_corr", data=spot_df["longitude"])
            if "ensemble" in spot_df:
                beam_group.create_dataset("class_ph", data=spot_df["ensemble"].astype(np.int16))
            for classifier in input_files["classifiers"]:
                beam_group.create_dataset(classifier, data=spot_df[classifier].astype(np.int16))

        # orbit info        
        orbit_group = hf.create_group("orbit_info")
        orbit_group.create_dataset("rgt", data=granule_info["rgt"])
        orbit_group.create_dataset("cycle_number", data=granule_info["cycle"])

    print("HDF5 file written: " + atl24_filename)
