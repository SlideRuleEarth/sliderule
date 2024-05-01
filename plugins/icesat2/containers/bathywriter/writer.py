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
#       "beam_csv_files": [<beam 1 file>, <beam 2 file>, ...],
#       "beam_json_files": [<beam 1 file>, <beam 2 file>, ...],
#       "openoceans_csv_files": [<beam 1 file>, <beam 2 file>, ...],
#       "output_file_name": <local output file>,
#       "output":
#       {
#           ... arrow parms
#       }
#   }
control_json = sys.argv[1]
with open(control_json, 'r') as json_file:
    control = json.load(json_file)

# read in data
beam_dfs = [pd.read_csv(beam_csv_file) for beam_csv_file in control["beam_csv_files"]]
beam_dicts = [json.load(open(beam_json_file, 'r')) for beam_json_file in control["beam_json_files"]]
openoceans_dfs = [pd.read_csv(openocean_csv_file) for openocean_csv_file in control["openoceans_csv_files"]]

# merge dataframes
for i in range(len(beam_dfs)):
    beam_dfs[i] = pd.merge(beam_dfs[i], openoceans_dfs[i], on='index_ph', how='left')

# get granule level info
granule_info = {
    "rgt": beam_dicts[0]["rgt"],
    "sc_orient": beam_dicts[0]["sc_orient"],
    "region": beam_dicts[0]["region"],
    "cycle": beam_dicts[0]["cycle"]
}

# Non-HDF5 Outputs
if "output" not in control or control["output"]["format"] != "hdf5":

    # add metadata columns to dataframe
    for i in range(len(beam_dfs)):
        for key in beam_dicts[i]:
            beam_dfs[i][key] = beam_dicts[i][key]

    # concatenate (vertically) all dataframes
    df = pd.concat(beam_dfs)

    # write output
    if "output" not in control or control["output"]["format"] == "csv":
        df.to_csv(control["output_file_name"])
    elif control["output"]["format"] == "parquet":
        if control["output"]["as_geo"]:
            df['time'] = df['time'].astype('datetime64[ns]')
            geometry = gpd.points_from_xy('longitude', 'latitude', 'geoid_corr_h')
            df.drop(columns=['longitude', 'latitude'], inplace=True)
            gdf = gpd.GeoDataFrame(df, geometry=geometry, crs="EPSG:7912")
            gdf.set_index('time', inplace=True)
            gdf.sort_index(inplace=True)
            gdf.to_parquet(control["output_file_name"], index=None)
        else:
            df.to_parquet(control["output_file_name"], index=None)

# HDF5 Output
else:

    with h5py.File(control["output_file_name"], 'w') as hf:

        # ancillary data
        ancillary_group = hf.create_group("ancillary_data")
        ancillary_group.create_dataset("atlas_sdp_gps_epoch", data=ATLAS_GPS_EPOCH)

        # beams
        for i in range(len(beam_dfs)):
            beam_info = beam_dicts[i]
            beam_df = beam_dfs[i]
            beam_group = hf.create_group(beam_info["beam"]) # e.g. gt1r, gt2l, etc.
            beam_group.create_dataset("index_ph", data=beam_df["index_ph"])            
            beam_df["delta_time"] = (beam_df["time"] / 1000000000.0) - ATLAS_GPS_EPOCH
            beam_group.create_dataset("delta_time", data=beam_df["delta_time"])
            beam_group.create_dataset("x_atc_corr", data=beam_df["x_atc"])
            beam_group.create_dataset("y_atc_corr", data=beam_df["y_atc"])
            beam_group.create_dataset("lat_corr", data=beam_df["latitude"])
            beam_group.create_dataset("lon_corr", data=beam_df["longitude"])
            beam_group.create_dataset("class_ph", data=beam_df["class_ph"])

        # orbit info        
        orbit_group = hf.create_group("orbit_info")
        orbit_group.create_dataset("rgt", data=granule_info["rgt"])
        orbit_group.create_dataset("sc_orient", data={"forward": 1, "backward": 0}[granule_info["sc_orient"]])
        orbit_group.create_dataset("cycle_number", data=granule_info["cycle"])
