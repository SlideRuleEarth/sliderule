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

import os
import json

#
# Constants
#
INPUT_CONTROL_FILENAME = "in.json"
OUTPUT_CONTROL_FILENAME = "out.json"
HOST_SHARED_DIRECTORY = "/usr/local/share/applications"

#
# Get Input Files
#
def get_input_files():
    input_files = []
    try:
        with open(os.join(HOST_SHARED_DIRECTORY, INPUT_CONTROL_FILENAME), 'r') as file:
            data = json.load(file)
            input_files = data["input_files"]
    except Exception as e:
        print(f'Failed to get input files: {e}')
    return input_files

#
# Get Output Directory
#
def get_output_directory():
    return HOST_SHARED_DIRECTORY

#
# Set Output Files
#
def set_output_files(output_files=[]):
    try:
        data = {"output_files": output_files}
        with open(os.join(HOST_SHARED_DIRECTORY, OUTPUT_CONTROL_FILENAME), 'w') as file:
            json.dump(a, file)
    except Exception as e:
        print(f'Failed to set output files: {e}')

