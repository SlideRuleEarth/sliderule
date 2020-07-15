#
# System Imports
#
import sys

import pandas as pd
import numpy as np
import math

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
# Distance between two coordinates
#
def geodist(lat1, lon1, lat2, lon2):

    lat1 = math.radians(lat1)
    lon1 = math.radians(lon1)
    lat2 = math.radians(lat2)
    lon2 = math.radians(lon2)

    dlon = lon2 - lon1  
    dlat = lat2 - lat1 

    dist = math.sin(dlat / 2)**2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2)**2
 #   dist = 2.0 * math.asin(math.sqrt(dist))  
    dist = 2.0 * math.atan2(math.sqrt(dist), math.sqrt(1.0 - dist))
    dist = 6373.0 * dist

    return dist

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

    # Process Response Data
    latitudes = [rsps[r]["ELEVATION"][i]["LAT"] for r in range(len(rsps)) for i in range(len(rsps[r]["ELEVATION"]))]
    longitudes = [rsps[r]["ELEVATION"][i]["LON"] for r in range(len(rsps)) for i in range(len(rsps[r]["ELEVATION"]))]
    heights = [rsps[r]["ELEVATION"][i]["HEIGHT"] for r in range(len(rsps)) for i in range(len(rsps[r]["ELEVATION"]))]

    # Build Dataframe of SlideRule Responses
    lat_origin = latitudes[0]
    lon_origin = longitudes[0]
    dist = [geodist(lat_origin, lon_origin, latitudes[i], longitudes[i]) for i in range(len(heights))]
    df = pd.DataFrame(data=heights, index=dist, columns=["sliderule"])

    # Return DataFrame
    print("Retrieved {} points from SlideRule".format(len(heights)))
    return df

#
# Parse Responses from Dataset
#
def recoverdata(rsps):
    datatype = rsps[0]["DATATYPE"]
    data = ()
    size = 0
    for d in rsps:
        data = data + d["DATA"] 
        size = size + d["SIZE"]
    return sliderule.get_values(data, datatype, size)

#
# ATL06 (read-ICESat-2) Read Request
#
def expread():

    # Baseline Request
    rqst = {
        "filename": "/data/ATLAS/ATL06_20181019065445_03150111_003_01.h5",
        "datatype": sliderule.datatypes["REAL"],
        "id": 0
    }

    # Read ATL06 (read-ICESat-2) Heights
    rqst["dataset"] = "/gt1r/land_ice_segments/h_li"
    rsps = sliderule.engine("h5", rqst)
    heights = recoverdata(rsps)

    # Read ATL06 (read-ICESat-2) Latitudes
    rqst["dataset"] = "/gt1r/land_ice_segments/latitude"
    rsps = sliderule.engine("h5", rqst)
    latitudes = recoverdata(rsps)

    # Read ATL06 (read-ICESat-2) Longitudes
    rqst["dataset"] = "/gt1r/land_ice_segments/longitude"
    rsps = sliderule.engine("h5", rqst)
    longitudes = recoverdata(rsps)

    # Build Dataframe of SlideRule Responses
    lat_origin = latitudes[0]
    lon_origin = longitudes[0]
    dist = [geodist(lat_origin, lon_origin, latitudes[i], longitudes[i]) for i in range(len(heights))]
    df = pd.DataFrame(data=heights, index=dist, columns=["atl06"])

    # Filter Dataframe
    df = df[df["atl06"] < 25000.0]
    df = df[df["atl06"] > -25000.0]
    df = df[df.index < 4000.0]

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
    if len(sys.argv) > 1 and sys.argv[1] == "ro":
        # Skip algorithm and populate dummy dataframe
        act = pd.DataFrame(data=[0,1,2], index=[0,1,2], columns=["sliderule"])
    else:
        act = algoexec()

    # Read ATL06 Expected Results
    exp = expread()

    # Plot Dataframe
    p = figure(title="Actual vs. Expected", plot_height=500, plot_width=800)
    for data, name, color in zip([act, exp], ["sliderule", "atl06"], Spectral11[:2]):
        p.line(list(data.index), data[name], line_width=3, color=color, alpha=0.8, legend_label=name)
    p.legend.location = "top_left"
    p.legend.click_policy="hide"
    show(p)



