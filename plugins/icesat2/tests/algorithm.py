#
# Imports
#
import sys
import logging

import pandas as pd
import numpy as np
import math

import matplotlib.pyplot as plt
import cartopy

import icesat2

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
def algoexec(resource, asset):

    # Build ATL06 Request
    parms = {
       "poly": [{"lat": -80.0, "lon": -70.0}, 
                {"lat": -82.5, "lon": -70.0},
                {"lat": -82.5, "lon": -65.0},
                {"lat": -80.0, "lon": -65.0}],
        "cnf": 4,
        "ats": 20.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # Request ATL06 Data
    rsps = icesat2.atl06(parms, resource, asset, as_numpy=False)

    # Calculate Distances
    lat_origin = rsps["lat"][0]
    lon_origin = rsps["lon"][0]
    distances = [geodist(lat_origin, lon_origin, rsps["lat"][i], rsps["lon"][i]) for i in range(len(rsps["h_mean"]))]

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(data=list(zip(rsps["h_mean"], distances, rsps["lat"], rsps["lon"], rsps["spot"])), index=rsps["segment_id"], columns=["h_mean", "distance", "latitude", "longitude", "spot"])

    # Return DataFrame
    print("Reference Ground Tracks: {} to {}".format(min(rsps["rgt"]), max(rsps["rgt"])))
    print("Cycle: {} to {}".format(min(rsps["cycle"]), max(rsps["cycle"])))
    print("Retrieved {} points from SlideRule".format(len(rsps["h_mean"])))
    return df

#
# ATL06 Read Request
#
def expread(resource, asset):

    # Read ATL06 Data
    segments    = icesat2.h5("/gt1r/land_ice_segments/segment_id",  resource, asset)
    heights     = icesat2.h5("/gt1r/land_ice_segments/h_li",        resource, asset)
    latitudes   = icesat2.h5("/gt1r/land_ice_segments/latitude",    resource, asset)
    longitudes  = icesat2.h5("/gt1r/land_ice_segments/longitude",   resource, asset)

    # Build Dataframe of SlideRule Responses    
    lat_origin = latitudes[0]
    lon_origin = longitudes[0]
    distances = [geodist(lat_origin, lon_origin, latitudes[i], longitudes[i]) for i in range(len(heights))]
    df = pd.DataFrame(data=list(zip(heights, distances, latitudes, longitudes)), index=segments, columns=["h_mean", "distance", "latitude", "longitude"])

    # Filter Dataframe
    df = df[df["h_mean"] < 25000.0]
    df = df[df["h_mean"] > -25000.0]
    df = df[df["distance"] < 4000.0]

    # Return DataFrame
    print("Retrieved {} points from ATL06, returning {} points".format(len(heights), len(df.values)))
    return df


#
# Plot Actual vs Expected
#
def plotresults(act, exp):
    # Create Plot
    fig = plt.figure(num=None, figsize=(12, 6))

    # Plot Ground Tracks
    ax1 = plt.subplot(121,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Ground Tracks")
    ax1.plot(act["longitude"].values, act["latitude"].values, linewidth=1.5, color='r', zorder=3, transform=cartopy.crs.Geodetic())
    ax1.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax1.add_feature(cartopy.feature.LAKES)
    ax1.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())
    ax1.gridlines(xlocs=np.arange(-180.,181.,60.), ylocs=np.arange(-90.,91.,30.))

    # Plot Elevations
    ax2 = plt.subplot(122)
    ax2.set_title("Along Track Elevations")
    track1 = act[act["spot"].isin([1, 2])].sort_values(by=['distance'])
    standard = exp.sort_values(by=['distance'])
    ax2.plot(track1["distance"].values, track1["h_mean"].values, linewidth=1.0, color='b')
    ax2.plot(standard["distance"].values, standard["h_mean"].values, linewidth=1.0, color='g')

    # Show Plot
    plt.show()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # configure logging
    logging.basicConfig(level=logging.INFO)

    # Set URL #
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Set ATL03 Asset #
    atl03_asset = "atl03-local"
    if len(sys.argv) > 2:
        atl03_asset = sys.argv[2]

    # Set ATL06 Asset #
    atl06_asset = "atl06-local"
    if len(sys.argv) > 3:
        atl06_asset = sys.argv[3]

    # Set Resource #
    resource = "_20181019065445_03150111_003_01.h5"
    if len(sys.argv) > 4:
        resource = sys.argv[4]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Execute SlideRule Algorithm
    act = algoexec("ATL03"+resource, atl03_asset)

    # Read ATL06 Expected Results
    exp = expread("ATL06"+resource, atl06_asset)

    # Plot Actual vs. Expected
    plotresults(act, exp)