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

import sys

from BathyPathFinder import BathyPathSearch

# process command line
sys.path.append('../utils')
from command_line_processor import process_command_line
settings, spot_info, spot_df, output_csv, info_json = process_command_line(sys.argv, columns=["x_atc", "geoid_corr_h", "surface_h", "index_ph", "class_ph"])

# set configuration
tau             = settings.get('tau', 0.5) 
k               = settings.get('k', 15)
n               = settings.get('n', 99)
find_surface    = settings.get('find_surface', True)

# keep only photons below sea surface
bathy_df = spot_df
if not find_surface:
    bathy_df = spot_df.loc[(spot_df['geoid_corr_h'] < spot_df['surface_h']) & (spot_df['class_ph'] != 41)]

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
    