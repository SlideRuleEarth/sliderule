#
# Imports
#
import sys

import pandas as pd
import numpy as np
import math

import matplotlib.pyplot as plt
import cartopy

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
# SlideRule Processing Request
#
def algoexec(asset):

    # Build ATL06 Request
    rqst = {
        "asset" : asset,
        "resource": "ATL03_20181019065445_03150111_003_01.h5",
        "track": 0,
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

    # Calculate Distances
    lat_origin = latitudes[0]
    lon_origin = longitudes[0]
    distances = [geodist(lat_origin, lon_origin, latitudes[i], longitudes[i]) for i in range(len(heights))]

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(data=list(zip(heights, distances, latitudes, longitudes, beam)), index=segments, columns=["height", "distance", "latitude", "longitude", "beam"])

    # Return DataFrame
    print("Reference Ground Tracks: {} to {}".format(min(rgt), max(rgt)))
    print("Cycle: {} to {}".format(min(cycle), max(cycle)))
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

    # Set URL #
    if len(sys.argv) > 1:
        sliderule.set_url(sys.argv[1])

    # Set Asset #
    asset = "atl03-local"
    if len(sys.argv) > 2:
        asset = sys.argv[2]

    # Set Verbosity #
    sliderule.set_verbose(True)

    # Execute SlideRule Algorithm
    act = algoexec(asset)

    # Read ATL06 Expected Results
    exp = expread("atl03-local")

    # Create Plot
    fig = plt.figure(num=None, figsize=(12, 6))

    # Plot Ground Tracks
    ax1 = plt.subplot(121,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Ground Tracks")
    ax1.plot(act["longitude"].values,act["latitude"].values,linewidth=1.5,
        color='r',zorder=2, transform=cartopy.crs.Geodetic())
    # add coastlines with filled land and lakes
    ax1.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax1.add_feature(cartopy.feature.LAKES)
    ax1.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())
    ax1.gridlines(xlocs=np.arange(-180.,181.,60.), ylocs=np.arange(-90.,91.,30.))

    # Plot Elevations
    ax2 = plt.subplot(122)
    ax2.set_title("Along Track Elevations")
    track1 = act[act["beam"].isin([1, 2])].sort_values(by=['distance'])
#    track2 = act[act["beam"].isin([3, 4])].sort_values(by=['distance'])
#    track3 = act[act["beam"].isin([5, 6])].sort_values(by=['distance'])
    standard = exp.sort_values(by=['distance'])
    ax2.plot(track1["distance"].values, track1["height"].values, linewidth=1.0, color='b')
#    plt.plot(track2["distance"].values, track2["height"].values, linewidth=1.0, color='b')
#    plt.plot(track3["distance"].values, track3["height"].values, linewidth=1.0, color='b')
    ax2.plot(standard["distance"].values, standard["height"].values, linewidth=1.0, color='g')

    # Show Plot
    plt.show()
