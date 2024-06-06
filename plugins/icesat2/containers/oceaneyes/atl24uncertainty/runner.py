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

from k_retrieval import get_kd_values

import rasterio as rio
import pandas as pd
import numpy as np
import sys
import json
import os

######################
# READ ICESAT2 INPUTS
######################

# process command line
if len(sys.argv) == 5:
    settings_json   = sys.argv[1]
    info_json       = sys.argv[2]
    input_csv       = sys.argv[3]
    output_csv      = sys.argv[4]
elif len(sys.argv) == 4:
    settings_json   = None
    info_json       = sys.argv[1]
    input_csv       = sys.argv[2]
    output_csv      = sys.argv[3]
elif len(sys.argv) == 3:
    settings_json   = None
    info_json       = sys.argv[1]
    input_csv       = sys.argv[2]
    output_csv      = None
else:
    print("Incorrect parameters supplied: python runner.py [<settings json>] <input json spot file> <input csv spot file> [<output csv spot file>]")
    sys.exit()

# read settings json
if settings_json != None:
    with open(settings_json, 'r') as json_file:
        settings = json.load(json_file)
else:
    settings = {}

# read info json
with open(info_json, 'r') as json_file:
    info = json.load(json_file)

# read input csv
data = pd.read_csv(input_csv)

############################
# CREATE UNCERTAINTY TABLES
############################

# create lookup tables
THU = [
    pd.read_csv("/data/ICESat2_0deg_500000_AGL_0.022_mrad_THU.csv"),
    pd.read_csv("/data/ICESat2_1deg_500000_AGL_0.022_mrad_THU.csv"),
    pd.read_csv("/data/ICESat2_2deg_500000_AGL_0.022_mrad_THU.csv"),
    pd.read_csv("/data/ICESat2_3deg_500000_AGL_0.022_mrad_THU.csv"),
    pd.read_csv("/data/ICESat2_4deg_500000_AGL_0.022_mrad_THU.csv"),
    pd.read_csv("/data/ICESat2_5deg_500000_AGL_0.022_mrad_THU.csv")
]
TVU = [
    pd.read_csv("/data/ICESat2_0deg_500000_AGL_0.022_mrad_TVU.csv"),
    pd.read_csv("/data/ICESat2_1deg_500000_AGL_0.022_mrad_TVU.csv"),
    pd.read_csv("/data/ICESat2_2deg_500000_AGL_0.022_mrad_TVU.csv"),
    pd.read_csv("/data/ICESat2_3deg_500000_AGL_0.022_mrad_TVU.csv"),
    pd.read_csv("/data/ICESat2_4deg_500000_AGL_0.022_mrad_TVU.csv"),
    pd.read_csv("/data/ICESat2_5deg_500000_AGL_0.022_mrad_TVU.csv")
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
    table[condition].mean()

# create averaged Kd coefficient values
# COEF_LUT[pointing_angle][wind_speed][kd_range][uncertainty_table]
COEF_LUT = [ [ [ [ average_kd(uncertainty_table, pointing_angle, wind_speed, kd_range) 
    for uncertainty_table in UNCERTAINTY_TABLES ] 
        for kd_range in KD_RANGES ]
            for wind_speed in WIND_SPEEDS ]
                for pointing_angle in POINTING_ANGLES ]

COEF_MAP = { average_kd(uncertainty_table, pointing_angle, wind_speed, kd_range) 
    for uncertainty_table in UNCERTAINTY_TABLES
        for kd_range in KD_RANGES
            for wind_speed in WIND_SPEEDS
                for pointing_angle in POINTING_ANGLES }

################
# GET Kd VALUES
################

# get date of granule in form: "%Y-%m-%d_%H%MZ"
profile_date = f'{info["year"]}-{info["month"]}-{info["day"]}_0000Z'

# set output path to the directory holding the inputs
pathlist = input_csv.split(os.path.sep)
outpath = os.path.sep.join(pathlist[:-1])

# read KD from VIIRS
kd_out, req_ok = get_kd_values(profile_date, outpath=outpath, keep=False)

# sample KD values at each coordinate
coord_list = [(x, y) for x, y in zip(data.longitude, data.latitude)]
with rio.open(f'netcdf:{kd_out}:Kd_490') as kd_src:
    data["kd_490"] = [x[0] for x in kd_src.sample(coord_list)]
    data["kd_490"] = data["kd_490"] * kd_src.scales[0] + kd_src.offsets[0]

################################
# CALCULATE UNCERTAINTY INDICES
################################

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
subaqueous = data["geoid_corr_h"] < data["surface_h"]

###################################
# CALCULATE HORIZONTAL UNCERTAINTY
###################################

# get horizontal subaqueous uncertainty (hsu) calculation coefficients
data['coef_hsu'] = pd.Series(zip(data['pointing_index'], data['wind_index'], data['kd_range_index'], 0)).map(COEF_MAP).values

# calculate horizontal subaqueous uncertainty (hsu)
data["sigma_hsu"] = 0
data.loc[subaqueous, "sigma_hsu"] = data.loc[subaqueous].apply(lambda row: (row['coef_hsu'].a * pow(row['depth'], 2)) + (row['coef_hsu'].b * row['depth']) + row['coef_hsu'].c)

# calculate horizontal aerial uncertainty (hau)
data["sigma_hau"] = np.sqrt(np.square(data["sigma_along"]) + np.square(data["sigma_across"]))

# calculate total horizontal uncertainty (thu)
data["sigma_thu"] = np.sqrt(np.square(data["sigma_hsu"]) + np.square(data["sigma_hau"]))

#################################
# CALCULATE VERTICAL UNCERTAINTY
#################################

# get vertical subaqueous uncertainty (vsu) calculation coefficients
data['coef_vsu'] = pd.Series(zip(data['pointing_index'], data['wind_index'], data['kd_range_index'], 1)).map(COEF_MAP).values

# calculate vertical subaqueous uncertainty (vsu)
data["sigma_vsu"] = 0
data.loc[subaqueous, "sigma_vsu"] = data.loc[subaqueous].apply(lambda row: (row['coef_vsu'].a * pow(row['depth'], 2)) + (row['coef_vsu'].b * row['depth']) + row['coef_vsu'].c)

# calculate total vertical uncertainty (tvu) - sigma_h comes from ATL03 and is the height uncertainty
data["sigma_tvu"] = np.sqrt(np.square(data["sigma_vsu"]) + np.square(data["sigma_h"]))

################
# WRITE OUTPUTS
################

# remove intermediate columns
del data["kd_490"]
del data["pointing_index"]
del data["wind_index"]
del data["kd_range_index"]
del data["coef_hsu"]
del data["sigma_hsu"]
del data["sigma_hau"]
del data["coef_vsu"]
del data["sigma_vsu"]

# output results
if output_csv != None:
    data.to_csv(output_csv, index=False)
else:
    print(json.dumps(info, indent=4))
