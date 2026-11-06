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

import json
import time
import sys
import numpy as np
import pandas as pd

###############################################################################
# CLASSES
###############################################################################

#
# Runner
#
class Runner:

    #
    # constructor
    #
    def __init__ (self):
        self.args = sys.argv[1:]
        self.result_file = self.args[-1]
        self.result = {
            "status": True,
            "build": "local",
            "start": time.time(),
            "outputs": [],
            "messages": []
        }

    #
    # arguments
    #
    @property
    def arguments (self):
        return self.args

    #
    # message
    #
    def message(self, msg: str):
        print(msg)
        self.result["messages"].append(msg)

    #
    # output
    #
    def output(self, output: str):
        self.result["outputs"].append(output)

    #
    # fail
    #
    def fail(self):
        self.result["status"] = False

    #
    # succeed
    #
    def succeed(self):
        self.result["status"] = True

    #
    # finish
    #
    def finish(self):
        self.result["stop"] = time.time()
        with open(self.result_file, "w") as file:
            json.dump(Runner.json_safe(self.result), file, allow_nan=False)

    #
    # json_safe
    #
    @staticmethod
    def json_safe(o):
        """
        convert to json-native types with NaN/NaT as None;
        json's default hook can't do this since np.float64 subclasses float
        """
        if isinstance(o, dict):
            return {k: Runner.json_safe(v) for k, v in o.items()}
        if isinstance(o, (list, tuple)):
            return [Runner.json_safe(v) for v in o]
        if isinstance(o, np.ndarray):
            return Runner.json_safe(o.tolist())
        if pd.api.types.is_scalar(o) and pd.isna(o):
            return None
        if isinstance(o, pd.Timestamp):
            return o.isoformat()
        if isinstance(o, np.generic): # np.int64, np.float32, np.bool_, ...
            return o.item()
        return o
