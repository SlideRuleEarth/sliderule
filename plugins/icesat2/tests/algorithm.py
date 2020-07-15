#
# System Imports
#
import sys

import pandas as pd
import numpy as np

from bokeh.io import show
from bokeh.palettes import Spectral11
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource

#
# Import SlideRule
#
sys.path.append("/usr/local/etc/sliderule")

import sliderule

#
# Interactive Bokeh Plot of Actual vs Expected
#
def bokeh_compare(act, col_act, exp, col_exp):

    p = figure(title="Actual vs. Expected", plot_height=500, plot_width=800)

    y_act = act[col_act].values
    x_act = [i for i in range(len(y_act))]

    y_exp = exp[col_exp].values
    x_exp = [i for i in range(len(y_exp))]

    r = p.multi_line(xs=[x_act, x_exp],
                     ys=[y_act, y_exp],
                     line_color=Spectral11[0:2],
                     line_width=3)

    show(p)

#
# SlideRule Processing Request
#
def algoexec():

    # Build ATL06 Request
    rqst = {
        "filename": "/data/ATLAS/ATL03_20181019065445_03150111_003_01.h5",
        "track": 1,
        "stages": ["LSF"],
        "parms": {
            "cnf": 4,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "maxi": 1
        }
    }

    # Execute ATL06 Algorithm
    rsps = sliderule.engine("atl06", rqst)

    # Build Dataframe of SlideRule Responses
    heights = [rsps[r]["ELEVATION"][i]["HEIGHT"] for r in range(len(rsps)) for i in range(len(rsps[r]["ELEVATION"]))]
    df = pd.DataFrame(data=heights, index=[i for i in range(len(heights))], columns=["sliderule"])

    # Return DataFrame
    print("Retrieved {} points from SlideRule".format(len(heights)))
    return df

#
# ATL06 (read-ICESat-2) Read Request
#
def expread():

    # Read ATL06 (read-ICESat-2) Heights
    rqst = {
        "filename": "/data/ATLAS/ATL06_20181019065445_03150111_003_01.h5",
        "dataset": "/gt1r/land_ice_segments/h_li",
        "datatype": sliderule.datatypes["REAL"],
        "id": 0
    }

    # Execute H5 Retrieval
    rsps = sliderule.engine("h5", rqst)
    datatype = rsps[0]["DATATYPE"]
    data = ()
    size = 0
    for d in rsps:
        data = data + d["DATA"] 
        size = size + d["SIZE"]
    heights = sliderule.get_values(data, datatype, size)

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(data=heights, index=[i for i in range(len(heights))], columns=["atl06"])

    # Filter Heights
    df = df[df["atl06"] < 25000.0]
    df = df[df["atl06"] > -25000.0]

    # Return DataFrame
    print("Retrieved {} points from ATL06, returning {} points".format(len(heights), len(df.values)))
    return df

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Populate Record Definitions
    sliderule.populate("atl03rec")
    sliderule.populate("atl03rec.photons")
    sliderule.populate("atl06rec")
    sliderule.populate("atl06rec.elevation")
    sliderule.populate("h5dataset")

    # Execute SlideRule Algorithm
    act = algoexec()

    # Read ATL06 Expected Results
    exp = expread()

    # Plot Dataframe
    bokeh_compare(act, "sliderule", exp, "atl06")
