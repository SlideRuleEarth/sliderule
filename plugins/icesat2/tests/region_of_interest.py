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

###############################################################################
# LOCAL FUNCTIONS
###############################################################################

def process_atl06_algorithm(parms, asset):

    # Latch Start Time
    perf_start = time.perf_counter()

    # Request ATL06 Data
    rsps = icesat2.atl06p(parms, asset)

    # Latch Stop Time
    perf_stop = time.perf_counter()

    # Build DataFrame of SlideRule Responses
    df = pd.DataFrame(rsps)

    # Display Statistics
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time". format(perf_duration))
    print("Reference Ground Tracks: {}".format(df["rgt"].unique()))
    print("Cycles: {}".format(df["cycle"].unique()))
    print("Received {} elevations".format(len(df)))

    # Return DataFrame
    return df

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)
    
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

    # Configure SlideRule
    icesat2.init(url, False)

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

    # Get ATL06 Elevations
    atl06 = process_atl06_algorithm(parms, asset)

    # Get ATL03 Subsetted Segments
    parms["stages"] = ['SUB']
    atl03 = process_atl06_algorithm(parms, asset)

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

    # Plot SlideRule ATL06 Ground Tracks
    ax1 = plt.subplot(131,projection=cartopy.crs.PlateCarree())
    ax1.set_title("SlideRule Zoomed ATL06 Ground Tracks")
    ax1.scatter(atl06["lon"].values, atl06["lat"].values, s=2.5, c=atl06["h_mean"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax1.set_extent(extent,crs=cartopy.crs.PlateCarree())
    ax1.plot(box_lon, box_lat, linewidth=1.5, color='r', zorder=2, transform=cartopy.crs.Geodetic())


    # Plot SlideRule ATL03 Ground Tracks
    ax2 = plt.subplot(132,projection=cartopy.crs.PlateCarree())
    ax2.set_title("SlideRule Subsetted ATL03 Ground Tracks")
    ax2.scatter(atl03["lon"].values, atl03["lat"].values, s=2.5, c=atl03["count"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax2.set_extent(extent,crs=cartopy.crs.PlateCarree())
    ax2.plot(box_lon, box_lat, linewidth=1.5, color='r', zorder=2, transform=cartopy.crs.Geodetic())

    # Plot SlideRule Global View
    ax3 = plt.subplot(133,projection=cartopy.crs.PlateCarree())
    ax3.set_title("SlideRule Global Reference")
    ax3.scatter(atl06["lon"].values, atl06["lat"].values, s=2.5, color='r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax3.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax3.add_feature(cartopy.feature.LAKES)
    ax3.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())

    # Show Plot
    plt.show()