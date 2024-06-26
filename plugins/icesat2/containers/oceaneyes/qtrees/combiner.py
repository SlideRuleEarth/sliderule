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
    print("Not enough parameters: python combiner.py <spot csv file> <qtrees csv file>")
    sys.exit()

# set command line arguments
spot_csv_file = sys.argv[1]
qtrees_csv_file = sys.argv[2]

# read in data
spot_df = pd.read_csv(spot_csv_file)
qtrees_df = pd.read_csv(qtrees_csv_file)
print("Read all into data frame")

# merge qtrees predictions into spot dataframe
trimmed_qtrees_df = pd.DataFrame()
trimmed_qtrees_df["index_ph"] = qtrees_df["index_ph"]
trimmed_qtrees_df["class_ph"] = qtrees_df["prediction"]
trimmed_qtrees_df["surface_h"] = qtrees_df["surface_h"]
del spot_df["class_ph"]
del spot_df["surface_h"]
spot_df = pd.merge(spot_df, trimmed_qtrees_df, on="index_ph", how='left')

# write out new qtrees file
del trimmed_qtrees_df['surface_h']
trimmed_qtrees_df.to_csv(qtrees_csv_file, index=False)

# write out new spot file
spot_df.to_csv(spot_csv_file, index=False)
