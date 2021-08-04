#!/usr/bin/env python
u"""
algorithm.py
"""
#
# Imports
#
import sys
import logging
import time

import pandas as pd
import numpy as np
import math

import matplotlib.pyplot as plt
import cartopy

from sliderule import icesat2

#
# SlideRule Processing Request
#
def algoexec(resource, asset):

    # Build ATL06 Request
    parms = { "cnf": 4,
              "ats": 20.0,
              "cnt": 10,
              "len": 40.0,
              "res": 20.0,
              "maxi": 1 }

    # Request ATL06 Data
    perf_start = time.perf_counter()
    process_start = time.process_time()
    gdf = icesat2.atl06(parms, resource, asset)
    perf_stop = time.perf_counter()
    process_stop = time.process_time()
    perf_duration = perf_stop - perf_start
    process_duration = process_stop - process_start

    # Check Results #
    if len(gdf["h_mean"]) != 622412:
        print("Failed atl06-sr algorithm test - incorrect number of points returned: ", len(gdf["h_mean"]))
    else:
        print("Passed atl06-sr algorithm test")

    # Return DataFrame
    print("Completed in {:.3f} seconds of wall-clock time, and {:.3f} seconds of processing time". format(perf_duration, process_duration))
    print("Reference Ground Tracks: {} to {}".format(min(gdf["rgt"]), max(gdf["rgt"])))
    print("Cycle: {} to {}".format(min(gdf["cycle"]), max(gdf["cycle"])))
    print("Retrieved {} points from SlideRule".format(len(gdf["h_mean"])))
    return gdf

#
# ATL06 Read Request
#
def expread(resource, asset):

    # Read ATL06 Data
    heights     = icesat2.h5("/gt1r/land_ice_segments/h_li",        resource, asset)
    delta_time  = icesat2.h5("/gt1r/land_ice_segments/delta_time",  resource, asset)

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(data=list(zip(heights, delta_time)), index=delta_time, columns=["h_mean", "delta_time"])
    df['time'] = pd.to_datetime(np.datetime64(icesat2.ATLAS_SDP_EPOCH) + delta_time.astype('timedelta64[s]'))

    # Check Results #
    if len(df.values) != 121753:
        print("Failed h5 retrieval test - insufficient points returned: ", len(df.values))
    else:
        print("Passed h5 retrieval test")

    # Return DataFrame
    print("Retrieved {} points from ATL06".format(len(heights)))
    return df

#
# Plot Actual vs Expected
#
def plotresults(act, exp, atl06_present):
    # Create Plot
    fig = plt.figure(num=None, figsize=(12, 6))

    # Plot Ground Tracks
    ax1 = plt.subplot(121,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Ground Tracks")
    ax1.scatter(act.geometry.x, act.geometry.y, s=1.5, color='r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax1.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax1.add_feature(cartopy.feature.LAKES)
    ax1.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())
    ax1.gridlines(xlocs=np.arange(-180.,181.,60.), ylocs=np.arange(-90.,91.,30.))

    # Plot Elevations
    ax2 = plt.subplot(122)
    ax2.set_title("Along Track Elevations")
    track1 = act[act["spot"].isin([1, 2])].sort_values(by=['time'])
    ax2.plot(track1["time"].values, track1["h_mean"].values, linewidth=1.0, color='b')
    if atl06_present:
        standard = exp.sort_values(by=['time'])
        ax2.plot(standard["time"].values, standard["h_mean"].values, linewidth=1.0, color='g')

    # Show Plot
    plt.show()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = ["127.0.0.1"]
    atl03_asset = "atlas-local"
    atl06_asset = "atlas-local"

    # configure logging
    logging.basicConfig(level=logging.INFO)

    # Set URL #
    if len(sys.argv) > 1:
        url = sys.argv[1]
        atl03_asset = "atlas-s3"
        atl06_asset = "atlas-s3"

    # Set ATL03 Asset #
    if len(sys.argv) > 2:
        atl03_asset = sys.argv[2]

    # Set ATL06 Asset #
    if len(sys.argv) > 3:
        atl06_asset = sys.argv[3]

    # Set Resource #
    atl06_present = True
    resource = "_20181019065445_03150111_003_01.h5"
    if len(sys.argv) > 4:
        resource = sys.argv[4]
        atl06_present = False

    # Bypass service discovery if url supplied
    if len(sys.argv) > 5:
        if sys.argv[5] == "bypass":
            url = [url]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Execute SlideRule Algorithm
    act = algoexec("ATL03"+resource, atl03_asset)

    # Read ATL06 Expected Results
    if atl06_present:
        exp = expread("ATL06"+resource, atl06_asset)
    else:
        exp = None

    # Plot Actual vs. Expected
    plotresults(act, exp, atl06_present)