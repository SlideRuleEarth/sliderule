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
#
# ----------------------------------------------------------------------------
# The code below was adapted from
#
#   git@github.com:SlideRuleEarth/ut-ATL24-C-shelph.git
#
# with the associated license replicated here:
# ----------------------------------------------------------------------------
#
# TBD
#

import sys
import json
import copy
import importlib
import pandas as pd

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

sys.path.append('/cshelph')
MODEL = importlib.import_module('c_shelph')

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
surface_buffer  = settings.get('surface_buffer', -0.5) 
h_res           = settings.get('h_res', 0.5)
lat_res         = settings.get('lat_res', 0.001)
thresh          = settings.get('thresh', 0.5)
min_buffer      = settings.get('min_buffer', -80)
max_buffer      = settings.get('max_buffer', 5)

# read info json
with open(info_json, 'r') as json_file:
    beam_info = json.load(json_file)

##################
# RUN MODEL
##################

point_cloud = pd.read_csv(input_csv)
results = MODEL.c_shelph_classification(copy.deepcopy(point_cloud), 
                                        surface_buffer=surface_buffer,
                                        h_res=h_res, lat_res=lat_res, thresh=thresh,
                                        min_buffer=min_buffer, max_buffer=max_buffer,
                                        sea_surface_label=41,
                                        bathymetry_label=40)

df = pd.DataFrame()
df["index_ph"] = results['index_ph']
df["class_ph"] = results['classification']

if output_csv != None:
    df.to_csv(output_csv, index=False, columns=["index_ph", "class_ph"])
else:
    print(df.to_string(index=False, header=True, columns=['index_ph', 'class_ph']))
