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

from photon_refraction import photon_refraction

import pandas as pd
from pyproj import Transformer
import sys
import json

##############
# READ INPUTS
##############

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

# set configuration
n1 = settings.get('n1', 1.00029) 
n2 = settings.get('n2', 1.34116)

# read info json
with open(info_json, 'r') as json_file:
    info = json.load(json_file)

# read input csv
data = pd.read_csv(input_csv)

# calculation refraction corrections
dE, dN, dZ = photon_refraction(data["surface_h"], data["geoid_corr_h"], data["ref_az"], data["ref_el"], n1, n2)

# apply corrections
data["dE"] = dE
data["dN"] = dN
data["dZ"] = dZ
subaqueous = data["geoid_corr_h"] < data["surface_h"]
data.loc[subaqueous, 'x_ph'] += data.loc[subaqueous, 'dE']
data.loc[subaqueous, 'y_ph'] += data.loc[subaqueous, 'dN']
data.loc[subaqueous, 'geoid_corr_h'] += data.loc[subaqueous, 'dZ']

# correct latitude and longitude
transformer = Transformer.from_crs(f"EPSG:326{info['utm_zone']}", "EPSG:7912", always_xy=True)
data["geoloc_corr"] = data.apply(lambda row: transformer.transform(row['x_ph'], row['y_ph']), axis=1)
data["longitude"] = data.apply(lambda row: row['geoloc_corr'][0], axis=1)
data["latitude"] = data.apply(lambda row: row['geoloc_corr'][1], axis=1)

# calculate subaqueous depth
data["depth"] = 0.0
data.loc[subaqueous, 'depth'] = data.loc[subaqueous, 'surface_h'] - data.loc[subaqueous, 'geoid_corr_h']

# capture statistics
data_corr = data[data["geoid_corr_h"] < data["surface_h"]]
info["refraction"] = {
    "dE": {
        "min": data_corr["dE"].min(),
        "max": data_corr["dE"].max(),
        "avg": data_corr["dE"].mean()
    },
    "dN": {
        "min": data_corr["dN"].min(),
        "max": data_corr["dN"].max(),
        "avg": data_corr["dN"].mean()
    },
    "dZ": {
        "min": data_corr["dZ"].min(),
        "max": data_corr["dZ"].max(),
        "avg": data_corr["dZ"].mean()
    },
    "total_photons": len(data),
    "subaqueous_photons": len(data_corr),
    "maximum_depth": data_corr["depth"].max()
}
with open(info_json, 'w') as json_file:
    json_file.write(json.dumps(info))

# remove intermediate columns
del data["dE"]
del data["dN"]
del data["dZ"]
del data["geoloc_corr"]

# output results
if output_csv != None:
    data.to_csv(output_csv, index=False)
else:
    print(json.dumps(info, indent=4))
