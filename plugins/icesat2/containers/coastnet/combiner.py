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

import pandas as pd
import sys

import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)

# check command line parameters
if len(sys.argv) <= 2:
    print("Not enough parameters: python combiner.py <spot csv file> <coastnet csv file>")
    sys.exit()

# set command line arguments
spot_csv_file = sys.argv[1]
coastnet_csv_file = sys.argv[2]

# read in data
spot_df = pd.read_csv(spot_csv_file)
coastnet_df = pd.read_csv(coastnet_csv_file)
print("Read all inputs into data frames")

# merge dataframes
coastnet_df.rename(columns={'ph_index': 'index_ph', 'prediction': 'class_ph'}, inplace=True)
del coastnet_df["along_track_dist"]
del coastnet_df["geoid_corrected_h"]
del coastnet_df["manual_label"]
del coastnet_df["sea_surface_h"]
del coastnet_df["bathy_h"]
spot_df = pd.merge(spot_df, coastnet_df, on='index_ph', how='left')
spot_df.to_csv(spot_csv_file, index=False)
print("Merged CSV file written: " + spot_csv_file)
