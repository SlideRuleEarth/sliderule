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

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

from kd_retrieval import get_kd_values

import rasterio as rio
import pandas as pd
import numpy as np
import sys
import json

######################
# READ ICESAT2 INPUTS
######################

# process command line
sys.path.append('../utils')
from command_line_processor import process_command_line
settings, info, data, output_csv, info_json = process_command_line(sys.argv)

############################
# CREATE UNCERTAINTY TABLES
############################

print("Building uncertainty tables")

# create lookup tables
THU = [
    pd.read_csv("/data/ICESat2_0deg_500000_AGL_0.022_mrad_THU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_1deg_500000_AGL_0.022_mrad_THU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_2deg_500000_AGL_0.022_mrad_THU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_3deg_500000_AGL_0.022_mrad_THU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_4deg_500000_AGL_0.022_mrad_THU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_5deg_500000_AGL_0.022_mrad_THU.csv", engine='pyarrow')
]
TVU = [
    pd.read_csv("/data/ICESat2_0deg_500000_AGL_0.022_mrad_TVU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_1deg_500000_AGL_0.022_mrad_TVU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_2deg_500000_AGL_0.022_mrad_TVU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_3deg_500000_AGL_0.022_mrad_TVU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_4deg_500000_AGL_0.022_mrad_TVU.csv", engine='pyarrow'),
    pd.read_csv("/data/ICESat2_5deg_500000_AGL_0.022_mrad_TVU.csv", engine='pyarrow')
]
UNCERTAINTY_TABLES = [THU, TVU]
POINTING_ANGLES = [0, 1, 2, 3, 4, 5]
WIND_SPEEDS = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

#                 0             1             2             3            4
# Kd ranges     clear     clear-moderate   moderate    moderate-high    high
KD_RANGES = [(0.06, 0.10), (0.11, 0.17), (0.18, 0.25), (0.26, 0.32), (0.33, 0.36)]

# function that averages Kd values in a table
def average_kd(table, pointing, wind, kd_range):
    condition = (table[pointing]["Kd"] >= kd_range[0]) & (table[pointing]["Kd"] <= kd_range[1]) & (table[pointing]["Wind"] == wind)
    return table[pointing][condition].mean()

# create averaged Kd coefficient values
# COEF_MAP [(pointing_angle, wind_speed, kd_range, uncertainty_table)] => (a, b, c) 
COEF_MAP = { 
    (pointing_angle, wind_speed, kd_range_index, table_index): average_kd(UNCERTAINTY_TABLES[table_index], pointing_angle, wind_speed, KD_RANGES[kd_range_index]) 
        for table_index in range(len(UNCERTAINTY_TABLES))
            for kd_range_index in range(len(KD_RANGES))
                for wind_speed in WIND_SPEEDS
                    for pointing_angle in POINTING_ANGLES 
}

################
# GET Kd VALUES
################

print("Retrieving Kd values")

# get date of granule in form: "%Y-%m-%d_%H%MZ"
profile_date = f'{info["year"]}-{info["month"]}-{info["day"]}_0000Z'

# read KD from VIIRS
kd_out, req_ok = get_kd_values(profile_date, outpath="/tmp", keep=False)

# sample KD values at each coordinate
coord_list = [(x, y) for x, y in zip(data.lon_ph, data.lat_ph)]
with rio.open(f'netcdf:{kd_out}:Kd_490') as kd_src:
    data["kd_490"] = [x[0] for x in kd_src.sample(coord_list)]
    data["kd_490"] = data["kd_490"] * kd_src.scales[0] + kd_src.offsets[0]

################################
# CALCULATE UNCERTAINTY INDICES
################################

print("Calculating uncertainty lookup table indices")

# get pointing angle index
data["pointing_index"] = data["pointing_angle"].round().astype(np.int32)
data['pointing_index'] = data['pointing_index'].where(data['pointing_index'] > POINTING_ANGLES[-1], POINTING_ANGLES[-1])
data['pointing_index'] = data['pointing_index'].where(data['pointing_index'] < POINTING_ANGLES[0], POINTING_ANGLES[0])

# get wind speed index
data["wind_index"] = data["wind_v"].round().astype(np.int32)
data['wind_index'] = data['wind_index'].where(data['wind_index'] > WIND_SPEEDS[-1], WIND_SPEEDS[-1])
data['wind_index'] = data['wind_index'].where(data['wind_index'] < WIND_SPEEDS[0], WIND_SPEEDS[0])

# get kd_range index
def find_kd_range(kd_490):
    i = 0
    for kd_range in KD_RANGES:
        if kd_490 <= kd_range[1]:
            break
        i += 1
    if i == len(KD_RANGES):
        i -= 1
    return i
data['kd_range_index'] = data['kd_490'].apply(find_kd_range)

# get index of photons below sea surface
subaqueous = data["ortho_h"] < data["surface_h"]

###################################
# CALCULATE HORIZONTAL UNCERTAINTY
###################################

print("Calculating horizontal uncertainties")

# get horizontal subaqueous uncertainty (hsu) calculation coefficients
data['table_index'] = 0
data['coef_hsu'] = pd.Series(zip(data['pointing_index'], data['wind_index'], data['kd_range_index'], data['table_index'])).map(COEF_MAP).values

# calculate horizontal subaqueous uncertainty (hsu)
data["sigma_hsu"] = 0
data.loc[subaqueous, "sigma_hsu"] = data.loc[subaqueous].apply(lambda row: (row['coef_hsu'].a * pow(row['depth'], 2)) + (row['coef_hsu'].b * row['depth']) + row['coef_hsu'].c, axis=1)

# calculate horizontal aerial uncertainty (hau)
data["sigma_hau"] = np.sqrt(np.square(data["sigma_along"]) + np.square(data["sigma_across"]))

# calculate total horizontal uncertainty (thu)
data["sigma_thu"] = np.sqrt(np.square(data["sigma_hsu"]) + np.square(data["sigma_hau"]))

#################################
# CALCULATE VERTICAL UNCERTAINTY
#################################

print("Calculating vertical uncertainties")

# get vertical subaqueous uncertainty (vsu) calculation coefficients
data['table_index'] = 1
data['coef_vsu'] = pd.Series(zip(data['pointing_index'], data['wind_index'], data['kd_range_index'], data['table_index'])).map(COEF_MAP).values

# calculate vertical subaqueous uncertainty (vsu)
data["sigma_vsu"] = 0
data.loc[subaqueous, "sigma_vsu"] = data.loc[subaqueous].apply(lambda row: (row['coef_vsu'].a * pow(row['depth'], 2)) + (row['coef_vsu'].b * row['depth']) + row['coef_vsu'].c, axis=1)

# calculate total vertical uncertainty (tvu) - sigma_h comes from ATL03 and is the height uncertainty
data["sigma_tvu"] = np.sqrt(np.square(data["sigma_vsu"]) + np.square(data["sigma_h"]))

#############################
# CALCULATE PROCESSING FLAGS
#############################

print("Calculating processing flags")
data["max_sensor_depth"] = 1.8 / data["kd_490"]
data["flags"] = 0
data.loc[subaqueous & (data["kd_490"] > 0) & (data["depth"] > data["max_sensor_depth"]), "flags"] = 1

################
# WRITE OUTPUTS
################

print("Writing outputs")

# capture statistics
info["uncertainty"] = {
    "horizontal": {
        "min": data.loc[subaqueous, "sigma_hsu"].min(),
        "max": data.loc[subaqueous, "sigma_hsu"].max(),
        "avg": data.loc[subaqueous, "sigma_hsu"].mean()
    },
    "vertical": {
        "min": data.loc[subaqueous, "sigma_vsu"].min(),
        "max": data.loc[subaqueous, "sigma_vsu"].max(),
        "avg": data.loc[subaqueous, "sigma_vsu"].mean()
    },
    "total_photons": len(data),
    "subaqueous_photons": len(data.loc[subaqueous]),
    "avg_sensor_depth": 1.8 / data.loc[subaqueous & (data['kd_490'] > 0), 'kd_490'].mean(),
    "max_sensor_depth": 1.8 / data.loc[subaqueous & (data['kd_490'] > 0), 'kd_490'].min()
}
with open(info_json, 'w') as json_file:
    json_file.write(json.dumps(info))

# remove intermediate columns
# del data["kd_490"] - keep this column for CSV and Parquet-based outputs (diagnostic)
del data["pointing_index"]
del data["wind_index"]
del data["kd_range_index"]
del data["coef_hsu"]
del data["sigma_hsu"]
del data["sigma_hau"]
del data["coef_vsu"]
del data["sigma_vsu"]
del data['table_index']
del data['max_sensor_depth']

# output results
if output_csv != None:
    data.to_csv(output_csv, index=False)
else:
    print(json.dumps(info, indent=4))
