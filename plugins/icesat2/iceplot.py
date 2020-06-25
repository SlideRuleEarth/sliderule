#
# System Imports
#
import sys

import pandas as pd
import numpy as np

from bokeh.io import push_notebook, show, output_notebook
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource

#
# Import SlideRule
#
sys.path.append("/usr/local/etc/sliderule")

import sliderule

#
# Interactive Bokeh Plot of Dataframe Column
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
# ATL06 Result Processing
#
def atl06(filename, track, stages):

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
    track = 1
    stages = ["LSF"]

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


        # Override track
        elif sys.argv[parm + 1] == "--track":
            track = int(sys.argv[parm + 2])
            parm += 2

        # Unrecognized
        else:
            print("Unrecognized parameter: ", sys.argv[parm + 1])
            parm += 1

    # Populate Record Definitions
    sliderule.populate("atl03rec")
    sliderule.populate("atl03rec.photons")
    sliderule.populate("atl06rec")
    sliderule.populate("atl06rec.elevation")

    # Execute ATL06 algorithm
    rqst = {
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

#    t = sliderule.source("definition", {"rectype" : "atl06rec.elevation"})
#    print(t)

    rsps = sliderule.engine("atl06", rqst)

    # Build Dataframe

    print(rsps)

    #df = atl06(filename, track, stages)
    #dfbokeh(df, "atl06")

