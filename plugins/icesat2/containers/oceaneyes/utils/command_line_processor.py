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
import warnings
warnings.simplefilter(action='ignore', category=FutureWarning)
import sys
import json

def process_command_line(argv):

    # process command line
    if len(argv) == 5:
        settings_json   = argv[1]
        info_json       = argv[2]
        input_csv       = argv[3]
        output_csv      = argv[4]
    elif len(argv) == 4:
        settings_json   = None
        info_json       = argv[1]
        input_csv       = argv[2]
        output_csv      = argv[3]
    elif len(argv) == 3:
        settings_json   = None
        info_json       = argv[1]
        input_csv       = argv[2]
        output_csv      = None
    else:
        print("Incorrect parameters supplied: python runner.py [<settings json>] <input json spot file> <input csv spot file> [<output csv spot file>]")
        sys.exit()

    # read settings json
    if settings_json != None:
        if settings_json[0] == '{':
            settings = json.loads(settings_json)
        else:
            with open(settings_json, 'r') as json_file:
                settings = json.load(json_file)
    else:
        settings = {}

    # read info json
    with open(info_json, 'r') as json_file:
        info = json.load(json_file)

    # read input
    if input_csv.endswith(".csv"):
        data = pd.read_csv(input_csv)
    elif input_csv.endswith(".parquet"):
        data = pd.read_parquet(input_csv)
    elif input_csv.endswith(".geoparquet"):
        import geopandas as gpd
        data = gpd.read_parquet(input_csv)
    else:
        data = pd.DataFrame()
    
    # filter on spot (if necessary)
    if "spot" in data and "spot" in settings:
        data = data[data["spot"] == settings["spot"]]
        info = info["spot_"+str(settings["spot"])]
        print(f'Dataframe for spot {settings["spot"]} read from {input_csv}')
    else:
        print(f'Dataframe read from {input_csv}')

    # return
    return settings, info, data, output_csv, info_json
