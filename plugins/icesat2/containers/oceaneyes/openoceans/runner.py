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

############
# CONSTANTS
############

NO_VALUE = -9999

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
res_along_track = settings.get('res_along_track', 10) 
res_z           = settings.get('res_z', 0.2)
window_size     = settings.get('window_size', 11) # 3x overlap is not enough to filter bad daytime noise
range_z         = settings.get('range_z', [-50, 30]) # include at least a few meters more than 5m above the surface for noise estimation, key for daytime case noise filtering
verbose         = settings.get('verbose', False) # not really fully integrated, it's still going to print some recent debugging statements
photon_bins     = settings.get('photon_bins', False)
parallel        = settings.get('parallel', True)
use_ndwi        = settings.get('use_ndwi', False)
chunk_size      = settings.get('chunk_size', 65536) # number of photons to process at one time

# read info json
with open(info_json, 'r') as json_file:
    ph_info_all = json.load(json_file)

# read input csv
ph_data_all = pd.read_csv(input_csv)

##################
# BUILD DATAFRAME
##################

ph_data = pd.DataFrame()

ph_data["photon_index"] = ph_data_all["index_ph"] # integer position of photon within the h5 file
ph_data["x_ph"] = ph_data_all["x_atc"] - min(ph_data_all["x_atc"]) # total along track distance, normalized to the minimum value
ph_data["z_ph"] = ph_data_all["geoid_corr_h"] # geoid-corrected photon height

ph_data["quality_ph"] = ph_data_all["quality_ph"] # modelling_parallel.py needs thiss

ph_data["classification"] = NO_VALUE
ph_data["conf_background"] = NO_VALUE
ph_data["conf_surface"] = NO_VALUE
ph_data["conf_column"] = NO_VALUE
ph_data["conf_bathymetry"] = NO_VALUE
ph_data["subsurface_flag"] = NO_VALUE
ph_data["weight_surface"] = NO_VALUE
ph_data["weight_bathymetry"] = NO_VALUE

# if ndwi is available, filter out land
if use_ndwi and 'ndwi' in ph_data_all:
    ph_data = ph_data[ph_data_all.ndwi > 0]

print(ph_data)

########################
# BUILD INFO DICTIONARY
########################

beam_strength = {
    1: "strong",
    2: "weak",
    3: "strong", 
    4: "weak",
    5: "strong",
    6: "weak"
}

ph_info = {'beam_strength': beam_strength[ph_info_all["spot"]]}
print(ph_info)

################
# PROFILE CLASS
################

class Profile:
    def __init__(self, data=None, info=None):
        self.info = info
        self.data = data
        self.signal_finding = False
        self.class_labels = {
            "surface": 41,
            "column": 45,
            "bathymetry": 40,
            "background": 1,
            "unclassified": 0,
            "none": 0,
        }
    def label_help(self, user_input=None):
        return self.class_labels[user_input]

################
# EXECUTE MODEL
################

model_outputs = []
for start_row in range(0, len(ph_data), chunk_size):
    print(f'Processing rows {start_row} to {start_row + chunk_size} out of {len(ph_data)}')
    p_sr = Profile(data=ph_data.iloc[start_row:start_row+chunk_size], info=ph_info)
    mmp = ModelMakerP(res_along_track=res_along_track, res_z=res_z, window_size=window_size, range_z=range_z, verbose=verbose, photon_bins=photon_bins, parallel=parallel)
    m = mmp.process(p_sr, n_cpu_cores=8)
    model_outputs.append(m.profile.data)

################
# WRITE OUTPUTS
################

results = pd.concat(model_outputs, ignore_index=True)

ph_out = pd.DataFrame()
ph_out["index_ph"] = results.photon_index
ph_out["class_ph"] = results.classification

if output_csv != None:
    ph_out.to_csv(output_csv, index=False, columns=["index_ph", "class_ph"])
else:
    print(ph_out.to_string(index=False, header=True, columns=['index_ph', 'class_ph']))
