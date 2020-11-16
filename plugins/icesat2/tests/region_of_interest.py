import sys
import logging
import time
import json
import pandas as pd
import numpy as np
import cartopy
import matplotlib.pyplot as plt
from datetime import datetime
from sliderule import icesat2

#
# Configure Logging
#
icesat2_logger = logging.getLogger("sliderule.icesat2")
icesat2_logger.setLevel(logging.INFO)
ch = logging.StreamHandler()
ch.setLevel(logging.INFO)
icesat2_logger.addHandler(ch)
# logging.basicConfig(level=logging.INFO)

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Region of Interest #
    region_filename = sys.argv[1]
    with open(region_filename) as regionfile:
        region = json.load(regionfile)["region"]
        
    # Set URL #
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 2:
        url = sys.argv[2]

    # Set Asset #
    asset = "atl03-local"
    if len(sys.argv) > 3:
        asset = sys.argv[3]

    # Latch Start Time
    perf_start = time.perf_counter()

    # Build ATL06 Request
    parms = {
        "poly": region,
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # Request ATL06 Data
    rsps = icesat2.atl06p(parms, asset)

    # Latch Stop Time
    perf_stop = time.perf_counter()

    # Build Dataframe of SlideRule Responses
    df = pd.DataFrame(rsps)

    # Display Statistics
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time". format(perf_duration))
    print("Reference Ground Tracks: {}".format(df["rgt"].unique()))
    print("Cycles: {}".format(df["cycle"].unique()))
    print("Received {} elevations".format(len(df)))

    # Calculate Extent
    lons = [p["lon"] for p in region]
    lats = [p["lat"] for p in region]
    lon_margin = (max(lons) - min(lons)) * 0.1
    lat_margin = (max(lats) - min(lats)) * 0.1
    extent = (min(lons) - lon_margin, max(lons) + lon_margin, min(lats) - lat_margin, max(lats) + lat_margin)

    # Create Plot
    fig = plt.figure(num=None, figsize=(24, 12))
    box_lon = [e["lon"] for e in region]
    box_lat = [e["lat"] for e in region]

    # Plot SlideRule Ground Tracks
    ax1 = plt.subplot(121,projection=cartopy.crs.PlateCarree())
    ax1.set_title("SlideRule Zoomed Ground Tracks")
    ax1.scatter(df["lon"].values, df["lat"].values, s=2.5, c=df["h_mean"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax1.set_extent(extent,crs=cartopy.crs.PlateCarree())
    ax1.plot(box_lon, box_lat, linewidth=1.5, color='r', zorder=2, transform=cartopy.crs.Geodetic())

    # Plot SlideRule Global View
    ax2 = plt.subplot(122,projection=cartopy.crs.PlateCarree())
    ax2.set_title("SlideRule Global Reference")
    ax2.scatter(df["lon"].values, df["lat"].values, s=2.5, color='r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax2.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax2.add_feature(cartopy.feature.LAKES)
    ax2.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())

    # Show Plot
    plt.show()