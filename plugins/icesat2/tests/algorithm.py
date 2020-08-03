#
# Imports
#
import sys

import pandas as pd
import numpy as np
import math

from bokeh.io import show
from bokeh.palettes import Spectral11
from bokeh.plotting import figure
from bokeh.models import ColumnDataSource
from bokeh.tile_providers import Vendors, get_provider

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
    dist = 2.0 * math.atan2(math.sqrt(dist), math.sqrt(1.0 - dist))
    dist = 6373.0 * dist

    return dist

#
# Mercator coordinates from lat and lon
#
def geomerc(lat, lon):
    r_major = 6378137.000
    x = r_major * math.radians(lon)
    scale = x/lon
    y = 180.0/math.pi * math.log(math.tan(math.pi/4.0 +
        lat * (math.pi/180.0)/2.0)) * scale
    return (x, y)
#
# SlideRule Processing Request
#
def algoexec(asset):

    # Build ATL06 Request
    rqst = {
        "asset" : asset,
        "resource": "ATL03_20181019065445_03150111_003_01.h5",
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
    segments = [rsps[r]["elevation"][i]["seg_id"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
    latitudes = [rsps[r]["elevation"][i]["lat"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
    longitudes = [rsps[r]["elevation"][i]["lon"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
    heights = [rsps[r]["elevation"][i]["height"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
    rgt = [rsps[r]["elevation"][i]["rgt"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
    cycle = [rsps[r]["elevation"][i]["cycle"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]
    beam = [rsps[r]["elevation"][i]["beam"] for r in range(len(rsps)) for i in range(len(rsps[r]["elevation"]))]

    # Print Meta Information
    print("Reference Ground Tracks: {} to {}".format(min(rgt), max(rgt)))
    print("Cycle: {} to {}".format(min(cycle), max(cycle)))
    print("Beam: {} to {}".format(min(beam), max(beam)))

    # Calculate Distances
    lat_origin = latitudes[0]
    lon_origin = longitudes[0]
    distances = [geodist(lat_origin, lon_origin, latitudes[i], longitudes[i]) for i in range(len(heights))]

    # Calculate Mercator Coordinates
    coord_x = [geomerc(latitudes[i], longitudes[i])[0] for i in range(len(heights))]
    coord_y = [geomerc(latitudes[i], longitudes[i])[1] for i in range(len(heights))]

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(data=list(zip(heights, distances, coord_y, coord_x)), index=segments, columns=["height", "distance", "y", "x"])

    # Return DataFrame
    print("Retrieved {} points from SlideRule".format(len(heights)))
    return df

#
# Parse Responses from Dataset
#
def recoverdata(rsps):
    datatype = rsps[0]["datatype"]
    data = ()
    size = 0
    for d in rsps:
        data = data + d["data"]
        size = size + d["size"]
    return sliderule.get_values(data, datatype, size)

#
# ATL06 (read-ICESat-2) Read Request
#
def expread(asset):

    # Baseline Request
    rqst = {
        "asset" : asset,
        "resource": "ATL06_20181019065445_03150111_003_01.h5",
        "datatype": sliderule.datatypes["REAL"],
        "id": 0
    }

    # Read ATL06 (read-ICESat-2) Segments
    rqst["dataset"] = "/gt1r/land_ice_segments/segment_id"
    rsps = sliderule.engine("h5", rqst)
    segments = recoverdata(rsps)

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
    distances = [geodist(lat_origin, lon_origin, latitudes[i], longitudes[i]) for i in range(len(heights))]
    df = pd.DataFrame(data=list(zip(heights, distances)), index=segments, columns=["height", "distance"])

    # Filter Dataframe
    df = df[df["height"] < 25000.0]
    df = df[df["height"] > -25000.0]
    df = df[df["distance"] < 4000.0]

    # Return DataFrame
    print("Retrieved {} points from ATL06, returning {} points".format(len(heights), len(df.values)))
    return df

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    if len(sys.argv) > 1:
        sliderule.set_url(sys.argv[1])

    asset = "atl03-local"
    if len(sys.argv) > 2:
        asset = sys.argv[2]

    # Populate Record Definitions
    sliderule.populate("atl03rec")
    sliderule.populate("atl03rec.photons")
    sliderule.populate("atl06rec")
    sliderule.populate("atl06rec.elevation")
    sliderule.populate("h5dataset")

    # Execute SlideRule Algorithm
    act = algoexec(asset)

    # Read ATL06 Expected Results
    exp = expread(asset)

    # Plot ground tracks
    tile_provider = get_provider(Vendors.CARTODBPOSITRON)
    ymin = act["y"].min()
    ymin = ymin - (abs(ymin) * 0.01)
    xmin = act["x"].min()
    xmin = xmin - (abs(xmin) * 0.01)
    ymax = act["y"].max()
    ymax = ymax + (abs(ymax) * 0.01)
    xmax = act["x"].max()
    xmax = xmax + (abs(xmax) * 0.01)
    m = figure(x_range=(xmin, xmax), y_range=(ymin, ymax), x_axis_type="mercator", y_axis_type="mercator")
    m.circle(act["x"], act["y"], color = 'red', alpha = 0.5, size = 10)
    m.add_tile(tile_provider)
    show(m)

    # Plot Elevations
    p = figure(title="Actual vs. Expected", plot_height=500, plot_width=800)
    for data, name, color in zip([act, exp], ["sliderule", "atl06"], Spectral11[:2]):
        p.line(data["distance"], data["height"], line_width=3, color=color, alpha=0.8, legend_label=name)
    p.legend.location = "top_left"
    p.legend.click_policy="hide"
    show(p)
