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
# ATL03 Photon Cloud Request
#
def phread(resource, asset):

    # Build Region Poly
    region = [
        { "lat": -80.75, "lon": -70.00 },
        { "lat": -81.00, "lon": -70.00 },
        { "lat": -81.00, "lon": -65.00 },
        { "lat": -80.75, "lon": -65.00 },
        { "lat": -80.75, "lon": -70.00 }
    ]

    # Build ATL06 Request
    parms = { "poly": region,
              "cnf": icesat2.CNF_SURFACE_HIGH,
              "ats": 20.0,
              "cnt": 10,
              "len": 40.0,
              "res": 20.0,
              "maxi": 1 }

    # Request ATL06 Data
    perf_start = time.perf_counter()
    process_start = time.process_time()
    gdf = icesat2.atl03s(parms, resource, asset, track=1)
    perf_stop = time.perf_counter()
    process_stop = time.process_time()
    perf_duration = perf_stop - perf_start
    process_duration = process_stop - process_start

    # Return DataFrame
    print("Completed in {:.3f} seconds of wall-clock time, and {:.3f} seconds of processing time". format(perf_duration, process_duration))
    print("Reference Ground Tracks: {} to {}".format(min(gdf["rgt"]), max(gdf["rgt"])))
    print("Cycle: {} to {}".format(min(gdf["cycle"]), max(gdf["cycle"])))
    print("Retrieved {} points from SlideRule".format(len(gdf["height"])))
    return gdf

#
# Plot Actual vs Expected
#
def plotresults(act, exp, ph):
    # Create Plot
    fig = plt.figure(num=None, figsize=(12, 6))

    # Plot Ground Tracks
    ax1 = plt.subplot(131,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Ground Tracks")
    ax1.scatter(act.geometry.x, act.geometry.y, s=1.5, color='r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax1.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax1.add_feature(cartopy.feature.LAKES)
    ax1.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())
    ax1.gridlines(xlocs=np.arange(-180.,181.,60.), ylocs=np.arange(-90.,91.,30.))

    # Plot Elevations
    ax2 = plt.subplot(132)
    ax2.set_title("Along Track Elevations")
    gt1r = act[act["gt"] == icesat2.GT1R].sort_values(by=['time'])
    ax2.plot(gt1r["time"].values, gt1r["h_mean"].values, linewidth=1.0, color='b', zorder=3)
    standard = exp.sort_values(by=['time'])
    ax2.plot(standard["time"].values, standard["h_mean"].values, linewidth=1.0, color='g', zorder=2)

    # Plot Photon Cloud
    ax3 = plt.subplot(133)
    ax3.set_title("Photon Cloud")
    gt1r = ph[ph["pair"] == icesat2.RIGHT_PAIR]
    ax3.scatter(gt1r.geometry.x, gt1r["height"].values, s=0.1, color='b')

    # Show Plot
    plt.show()

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = ["127.0.0.1"]
    atl03_asset = "atlas-local"
    atl06_asset = "atlas-local"
    resource = "_20181019065445_03150111_003_01.h5"

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

    # Bypass service discovery if url supplied
    if len(sys.argv) > 4:
        if sys.argv[4] == "bypass":
            url = [url]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Execute SlideRule Algorithm
    act = algoexec("ATL03"+resource, atl03_asset)

    # Read ATL06 Expected Results
    exp = expread("ATL06"+resource, atl06_asset)

    # Read ATL03 Photon Cloud
    ph = phread("ATL03"+resource, atl03_asset)

    # Plot Actual vs. Expected
    plotresults(act, exp, ph)