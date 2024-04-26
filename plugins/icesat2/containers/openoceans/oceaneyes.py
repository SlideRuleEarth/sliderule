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
# The code below was adapted from https://github.com/jonm3D/openoceans-dev.git
# with the associated license replicated here:
# ----------------------------------------------------------------------------
#
# Copyright (c) 2022, Jonathan Markel/UT Austin.
# 
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
# 
# Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution.
#
# Neither the name of the copyright holder nor the names of its 
# contributors may be used to endorse or promote products derived from this 
# software without specific prior written permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR '
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import pandas as pd
import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)
from modeling_parallel import ModelMakerP # classes for modeling bathymetry from the profile
import sys
import json

###################
# GET INPUTS
###################

if len(sys.argv) > 3:
    ph_data_all = pd.read_csv(sys.argv[1])
    with open(sys.argv[2], 'r') as json_file:
        ph_info_all = json.load(json_file)
    output_filename = sys.argv[3]
else:
    print("Not enough parameters: python oceaneyes.py <input csv beam file> <input json beam file> <output csv beam file>")
    sys.exit()

# read in the csv file
print(ph_data_all.columns)

# photon index - integer position of photon within the h5 file
# x_ph - total along track distance, normalized to the minimum value
# z_ph - geoid-corrected photon height
input_variables = ['photon_index', 'x_ph', 'z_ph']

# storing photon classifications 
openoceans_prepopulated = ['classification',
       'conf_background', 'conf_surface', 'conf_column', 'conf_bathymetry',
       'subsurface_flag', 'weight_surface', 'weight_bathymetry']

# subset the example dataframe
ph_data = ph_data_all.loc[:, input_variables + openoceans_prepopulated]


class Profile:

    def __init__(self, data=None, info=None):
        self.info = info
        self.data = data
        self.signal_finding = False
        self.class_labels = {
            "surface": 41,
            "column": 45,
            "bathymetry": 40,
            "background": 0,
            "unclassified": -1,
            "none": -9999,
        }

    def label_help(self, user_input=None):
        return self.class_labels[user_input]

p_sr = Profile(data=ph_data, info=ph_info_all)
print(p_sr.data.head())

# if ndwi is available, filter out land
if 'ndwi' in p_sr.data.columns:
    p_sr.data = p_sr.data[p_sr.data.ndwi > 0]

# filter dataframe 
print(p_sr.data.shape)

# input parameters may vary for different models
Mp = ModelMakerP(res_along_track=10, 
                    res_z=0.2,
                    window_size=11, # 3x overlap is not enough to filter bad daytime noise
                    range_z=[-50, 30], # include at least a few meters more than 5m above the surface for noise estimation, key for daytime case noise filtering
                    verbose=False, # not really fully integrated, it's still going to print some recent debugging statements
                    photon_bins=False,
                    parallel=True)

# for reference, this takes about 1.5 minutes on my Apple Silicon Macbook Pro using all 10 cores for parallel processing
# I'm not sure how/if the parallelization will work on SlideRule's servers? 
# It can also run in serial (try 'process_orig' instead of 'process' for the original, serial version), but that's unmanageably slow for my laptop
m = Mp.process(p_sr, n_cpu_cores=10)
print(m.profile.data.classification)

# write classifications to output file
p_sr.data["class_ph"] = m.profile.data.classification
columns = ["photon_index", "class_ph"]
p_sr.data.to_csv(output_filename, index=False, columns=columns)
