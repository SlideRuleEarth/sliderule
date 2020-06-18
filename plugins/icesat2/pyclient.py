#
# Imports
#
import sys
import requests
import json

import pandas as pd
import numpy as np

from bokeh.io import push_notebook, show, output_notebook
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource

#
# Interactive Bokeh Plot of Dataframe Column
# ... note that x axis is a recreated list of item number
# ... and does not reflect original index in dataframe
#
def dfbokeh(df, col, cond_col="", cond_val=0):
    p = figure(title=col, plot_height=500, plot_width=800)
    if cond_col != "":
        c = df[df[cond_col] == cond_val]
        s = c[pd.isnull(c[col]) == False][col]
    else:
        s = df[pd.isnull(df[col]) == False][col]
    y = s.values
    x = [i for i in range(len(y))]
    r = p.line(x, y, color="#2222aa", line_width=3)
    show(p, notebook_handle=True)

#
# H5 Endpoint
#
def h5endpoint (filename, dataset, datatype, difference):

    rqst_dict = {
        "filename": filename,
        "dataset": dataset,
        "id": 0
    }

    # Make API Request
    d = requests.post('%s/engine/h5' % (server_url), data=json.dumps(rqst_dict), stream=True)

    # Read Response
    max_bytes = 0x80000
    response_bytes = 0
    responses = []
    for line in d.iter_content(0x10000):
        if line:
            response_bytes += len(line)
            responses.append(line)
            if response_bytes >= max_bytes:
                break

    # Build DataFrame
    raw = b''.join(responses)
    size = int(len(raw) / np.dtype(datatype).itemsize)
    values = np.frombuffer(raw, dtype=datatype, count=size)
    df = pd.DataFrame(data=values, index=[i for i in range(size)], columns=[dataset])
    if difference:
        df = df.diff(axis=0)

    # Return DataFrame
    return df

#
# ATL06 Endpoint
#
def atl06endpoint (filename, track, stages):

    rqst_dict = {
        "filename": filename,
        "track": track,
        "stages": stages,
        "parms": {
            "cnf": 4,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0
        }
    }

    # Make API Request
    d = requests.post('%s/engine/atl06' % (server_url), data=json.dumps(rqst_dict), stream=True)

    # Read Response
    response_bytes = 0
    responses = []
    for line in d.iter_content(0x10000):
        if line:
            response_bytes += len(line)
            responses.append(line)

    # Build DataFrame
    raw = b''.join(responses)
    size = int(len(raw) / np.dtype(np.double).itemsize)
    values = np.frombuffer(raw, dtype=np.double, count=size)
    df = pd.DataFrame(data=values, index=[i for i in range(size)], columns=["atl06"])

    # Print Status #
    print('Processed %d data points' % (size))

    # Return DataFrame
    return df

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Default Parameters #
    server_url = 'http://127.0.0.1:9081'
    filename = "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5"
    dataset = "/gt1r/geolocation/segment_ph_cnt"
    datatype = np.int32
    track = 1
    stages = ["LSF"]
    difference = False

    # Process Command Line Arguments #
    parm = 0
    while parm + 1 < len(sys.argv):

        # Override server_url
        if sys.argv[parm + 1] == "--url":
            server_url = sys.argv[parm + 2]
            parm += 2

        # Override filename
        elif sys.argv[parm + 1] == "--file":
            filename = sys.argv[parm + 2]
            parm += 2

        # Override dataset
        elif sys.argv[parm + 1] == "--dataset":
            dataset = sys.argv[parm + 2]
            parm += 2

        # Override datatype
        elif sys.argv[parm + 1] == "--datatype":
            if sys.argv[parm + 2] == "int32":
                datatype = np.int32
            elif sys.argv[parm + 2] == "float":
                datatype = np.float
            elif sys.argv[parm + 2] == "double":
                datatype = np.double
            elif sys.argv[parm + 2] == "uint8":
                datatype = np.uint8
            parm += 2

        # Override track
        elif sys.argv[parm + 1] == "--track":
            track = int(sys.argv[parm + 2])
            parm += 2

        # Enable differencing
        elif sys.argv[parm + 1] == "--diff":
            difference = True
            parm += 1

        # Read H5 dataset
        elif sys.argv[parm + 1] == "--h5":
            df = h5endpoint(filename, dataset, datatype, difference)
            dfbokeh(df, dataset)
            parm += 1

        # Execute ATL06 algorithm
        elif sys.argv[parm + 1] == "--atl06":
            df = atl06endpoint(filename, track, stages)
            dfbokeh(df, "atl06")
            parm += 1

        # Unrecognized
        else:
            print("Unrecognized parameter: ", sys.argv[parm + 1])
            parm += 1
