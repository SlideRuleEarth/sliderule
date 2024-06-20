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

import logging
logging.basicConfig(level=logging.DEBUG)

import pandas as pd
import numpy as np
import sys
import json

from BathyPathFinder import BathyPathSearch

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
tau             = settings.get('tau', 0.5) 
k               = settings.get('k', 15)
n               = settings.get('n', 99)
find_surface    = settings.get('find_surface', True)

# read info json
with open(info_json, 'r') as json_file:
    spot_info = json.load(json_file)

# read input data into dataframe
print(f'Processing {input_csv}...')
spot_df = pd.read_csv(input_csv)

# keep only photons below sea surface
bathy_df = spot_df
if not find_surface:
    bathy_df = spot_df.loc[spot_df['geoid_corr_h'] < spot_df['surface_h']]

# run bathy pathfinder
bps = BathyPathSearch(tau, k, n)
bps.fit(bathy_df['x_atc'], bathy_df['geoid_corr_h'], find_surface)

# write bathy classifications to spot df
spot_df.loc[bps.bathy_photons.index, 'class_ph'] = 40

# write (or print) output
if output_csv != None:
    spot_df.to_csv(output_csv, index=False, columns=["index_ph", "class_ph"])
else:
    print(spot_df.to_string(index=False, header=True, columns=['index_ph', 'class_ph']))
    