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
# Parameters
#
server_url = 'http://127.0.0.1:9081'

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
# SlideRule API
#
def engine (api, parm):
    url = '%s/engine/%s' % (server_url, api)
    return requests.post(url, data=parm, stream=True)

#
# H5 Endpoint
#
def h5endpoint (filename, dataset, datatype):

    # Make API Request
    rqst_dict = {
        "filename": filename,
        "dataset": dataset,
        "id": 0
    }
    d = engine("h5", json.dumps(rqst_dict))

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

    # Return DataFrame
    return df

#
# ATL06 Endpoint
#
def atl06endpoint (filename, track):

    # Make API Request
    rqst_dict = {
        "filename": filename,
        "track": track,
        "id": 0
    }
    d = engine("atl06", json.dumps(rqst_dict))

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
    size = int(len(raw) / np.dtype(np.double).itemsize)
    values = np.frombuffer(raw, dtype=np.double, count=size)
    df = pd.DataFrame(data=values, index=[i for i in range(size)], columns=["atl06"])

    # Return DataFrame
    return df

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    parm = 0
    while parm + 1 < len(sys.argv):
        # Override server URL from command line
        if sys.argv[parm + 1] == "--url":
            server_url = sys.argv[parm + 2]
            parm += 2
        # H5 Endpoint Example
        elif sys.argv[parm + 1] == "--h5":
            filename = "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5"
            dataset = "/gt1r/geolocation/segment_ph_cnt"
            datatype = np.int32
            df = h5endpoint(filename, dataset, datatype)
            dfbokeh(df, dataset)
            parm += 1
        # H5 Endpoint Example
        elif sys.argv[parm + 1] == "--atl06":
            filename = "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5"
            track = 1
            df = atl06endpoint(filename, track)
            dfbokeh(df, "atl06")
            parm += 1
