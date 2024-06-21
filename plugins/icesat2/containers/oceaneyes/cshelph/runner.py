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
sys.path.append('../utils')
from command_line_processor import process_command_line
settings, beam_info, point_cloud, output_csv, info_json = process_command_line(sys.argv)

# set configuration
surface_buffer  = settings.get('surface_buffer', -0.5) 
h_res           = settings.get('h_res', 0.5)
lat_res         = settings.get('lat_res', 0.001)
thresh          = settings.get('thresh', 0.5)
min_buffer      = settings.get('min_buffer', -80)
max_buffer      = settings.get('max_buffer', 5)

##################
# RUN MODEL
##################

results = MODEL.c_shelph_classification(copy.deepcopy(point_cloud), 
                                        surface_buffer=surface_buffer,
                                        h_res=h_res, lat_res=lat_res, thresh=thresh,
                                        min_buffer=min_buffer, max_buffer=max_buffer,
                                        sea_surface_label=41,
                                        bathymetry_label=40)

################
# WRITE OUTPUTS
################

df = pd.DataFrame()
df["index_ph"] = point_cloud['index_ph']
df["class_ph"] = results['classification']

if output_csv != None:
    df.to_csv(output_csv, index=False, columns=["index_ph", "class_ph"])
else:
    print(df.to_string(index=False, header=True, columns=['index_ph', 'class_ph']))
