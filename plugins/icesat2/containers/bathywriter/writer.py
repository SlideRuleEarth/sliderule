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
    print("Not enough parameters: python bathywriter.py <control json file>")
    sys.exit()

# read control json
#   {
#       "spot_csv_files": [<spot 1 file>, <spot 2 file>, ...],
#       "spot_json_files": [<spot 1 file>, <spot 2 file>, ...],
#       "openoceans_csv_files": [<spot 1 file>, <spot 2 file>, ...],
#       "output_filename": <local output file>,
#       "output":
#       {
#           ... arrow parms
#       }
#   }
control_json = sys.argv[1]
with open(control_json, 'r') as json_file:
    control = json.load(json_file)

# read in data
spot_dfs = [pd.read_csv(spot_csv_file) for spot_csv_file in control["spot_csv_files"]]
spot_dicts = [json.load(open(spot_json_file, 'r')) for spot_json_file in control["spot_json_files"]]
openoceans_dfs = [pd.read_csv(openocean_csv_file) for openocean_csv_file in control["openoceans_csv_files"]]
print("Read all inputs into data frames")

# merge dataframes
for i in range(len(spot_dfs)):
    spot_dfs[i] = pd.merge(spot_dfs[i], openoceans_dfs[i], on='index_ph', how='left')
print("Created merged spot data frames")

# get granule level info
granule_info = {
    "rgt": spot_dicts[0]["rgt"],
    "sc_orient": spot_dicts[0]["sc_orient"],
    "region": spot_dicts[0]["region"],
    "cycle": spot_dicts[0]["cycle"]
}

# Non-HDF5 Outputs
if "output" not in control or control["output"]["format"] != "hdf5":

    # add metadata columns to dataframe
    for i in range(len(spot_dfs)):
        for key in spot_dicts[i]:
            spot_dfs[i][key] = spot_dicts[i][key]
    print("Added meta data columns")

    # concatenate (vertically) all dataframes
    df = pd.concat(spot_dfs)
    print("Concatenated data frames into a single data frame")

    # write output
    if "output" not in control or control["output"]["format"] == "csv":
        df.to_csv(control["output_filename"])
        print("CSV file written: " + control["output_filename"])
    elif control["output"]["format"] == "parquet":
        if control["output"]["as_geo"]:
            df['time'] = df['time'].astype('datetime64[ns]')
            geometry = gpd.points_from_xy('longitude', 'latitude', 'geoid_corr_h')
            df.drop(columns=['longitude', 'latitude'], inplace=True)
            gdf = gpd.GeoDataFrame(df, geometry=geometry, crs="EPSG:7912")
            gdf.set_index('time', inplace=True)
            gdf.sort_index(inplace=True)
            gdf.to_parquet(control["output_filename"], index=None)
            print("GeoParquet file written: " + control["output_filename"])
        else:
            df.to_parquet(control["output_filename"], index=None)
            print("Parquet file written: " + control["output_filename"])

# HDF5 Output
else:

    with h5py.File(control["output_filename"], 'w') as hf:

        # ancillary data
        ancillary_group = hf.create_group("ancillary_data")
        ancillary_group.create_dataset("atlas_sdp_gps_epoch", data=ATLAS_GPS_EPOCH)

        # spots
        for i in range(len(spot_dfs)):
            spot_info = spot_dicts[i]
            spot_df = spot_dfs[i]
            beam_group = hf.create_group(spot_info["beam"]) # e.g. gt1r, gt2l, etc.
            beam_group.create_dataset("index_ph", data=spot_df["index_ph"])            
            spot_df["delta_time"] = (spot_df["time"] / 1000000000.0) - ATLAS_GPS_EPOCH
            beam_group.create_dataset("delta_time", data=spot_df["delta_time"])
            beam_group.create_dataset("x_atc_corr", data=spot_df["x_atc"])
            beam_group.create_dataset("y_atc_corr", data=spot_df["y_atc"])
            beam_group.create_dataset("lat_corr", data=spot_df["latitude"])
            beam_group.create_dataset("lon_corr", data=spot_df["longitude"])
            beam_group.create_dataset("class_ph", data=spot_df["class_ph"].astype(np.int16))

        # orbit info        
        orbit_group = hf.create_group("orbit_info")
        orbit_group.create_dataset("rgt", data=granule_info["rgt"])
        orbit_group.create_dataset("sc_orient", data={"forward": 1, "backward": 0}[granule_info["sc_orient"]])
        orbit_group.create_dataset("cycle_number", data=granule_info["cycle"])

    print("HDF5 file written: " + control["output_filename"])
