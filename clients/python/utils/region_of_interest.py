#
# Requests SlideRule to process the provided region of interest twice: 
# (1) perform ATL06-SR algorithm to calculate elevations in region
# (2) retrieve photon counts from each ATL06 extent
#
# The region of interest is passed in as a json file that describes the
# geospatial polygon for the region. An example json object is below:
#
# {
#   "region": [{"lon": 86.269733430535638, "lat": 28.015965655545852},
#              {"lon": 86.261403224371804, "lat": 27.938668666352985},
#              {"lon": 86.302412923741514, "lat": 27.849318271202186},
#              {"lon": 86.269733430535638, "lat": 28.015965655545852}]
# }
#

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

def process_atl06_algorithm(parms, asset, max_workers, subset=False):

    # Latch Start Time
    perf_start = time.perf_counter()

    # Request ATL06 Data
    if not subset:
        rsps = icesat2.atl06p(parms, asset, 0, False, max_workers)
    else:
        rsps = icesat2.atl03sp(parms, asset, 0, max_workers)

    # Latch Stop Time
    perf_stop = time.perf_counter()

    # Build DataFrame of SlideRule Responses
    df = pd.DataFrame(rsps)

    # Display Statistics
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    print("Reference Ground Tracks: {}".format(df["rgt"].unique()))
    print("Cycles: {}".format(df["cycle"].unique()))
    print("Received {} elevations".format(len(df)))

    # Return DataFrame
    return df

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = ["127.0.0.1"]
    asset = "atlas-local"
    max_workers = 0

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)
    
    # Region of Interest #
    region_filename = sys.argv[1]
    regions = icesat2.toregion(region_filename)
    if(len(regions) > 1):
        print("Warning - {} regions detected, only first region supplied will be processed".format(len(regions)))
    elif(len(regions) < 1):
        print("Error - no valid regions supplied")
        exit()
    region = regions[0]

    # Set URL #
    if len(sys.argv) > 2:
        url = sys.argv[2]
        asset = "atlas-s3" # unlikely to use local if overriding url

    # Set Asset #
    if len(sys.argv) > 3:
        asset = sys.argv[3]

    # Set Maximum Workers #
    if len(sys.argv) > 4:
        max_workers = int(sys.argv[4])

    # Bypass service discovery if url supplied
    if len(sys.argv) > 5:
        if sys.argv[5] == "bypass":
            url = [url]

    # Configure SlideRule #
    icesat2.init(url, False)

    # Build ATL06 Request #
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
    atl06 = process_atl06_algorithm(parms, asset, max_workers)

    # Get ATL03 Subsetted Segments
    atl03 = process_atl06_algorithm(parms, asset, max_workers, subset=True)

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

    # Plot ATL06 Ground Tracks
    ax1 = plt.subplot(231,projection=cartopy.crs.PlateCarree())
    ax1.set_title("Zoomed ATL06 Ground Tracks")
    ax1.scatter(atl06["lon"].values, atl06["lat"].values, s=2.5, c=atl06["h_mean"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax1.set_extent(extent,crs=cartopy.crs.PlateCarree())
    ax1.plot(box_lon, box_lat, linewidth=1.5, color='r', zorder=2, transform=cartopy.crs.Geodetic())

    # Plot ATL03 Ground Tracks
    ax2 = plt.subplot(232,projection=cartopy.crs.PlateCarree())
    ax2.set_title("Subsetted ATL03 Ground Tracks")
    ax2.scatter(atl03["lon"].values, atl03["lat"].values, s=2.5, c=atl03["count"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax2.set_extent(extent,crs=cartopy.crs.PlateCarree())
    ax2.plot(box_lon, box_lat, linewidth=1.5, color='r', zorder=2, transform=cartopy.crs.Geodetic())

    # Plot Global View
    ax3 = plt.subplot(233,projection=cartopy.crs.PlateCarree())
    ax3.set_title("Global Reference")
    ax3.scatter(atl06["lon"].values, atl06["lat"].values, s=2.5, color='r', zorder=3, transform=cartopy.crs.PlateCarree())
    ax3.add_feature(cartopy.feature.LAND, zorder=0, edgecolor='black')
    ax3.add_feature(cartopy.feature.LAKES)
    ax3.set_extent((-180,180,-90,90),crs=cartopy.crs.PlateCarree())

    # Plot Number of Fit Photons per ATL06 Ground Tracks
    ax4 = plt.subplot(234)
    atl06.hist("n_fit_photons", bins=100, ax=ax4)
    ax4.set_title("Number of Fit Photons")

    # Plot Final Window Size per ATL06 Ground Tracks
    ax5 = plt.subplot(235)
    atl06.hist("w_surface_window_final", bins=100, ax=ax5)
    ax5.set_title("Final Window Size")

    # Plot Number of Fit Photons per ATL06 Ground Tracks
    ax6 = plt.subplot(236)
    atl06.hist("rms_misfit", bins=100, ax=ax6)
    ax6.set_title("RMS of Fit")

    # Plot Time of Measurement per ATL06 Ground Tracks
    #ax6 = plt.subplot(236,projection=cartopy.crs.PlateCarree())
    #ax6.set_title("Time of Measurement")
    #ax6.scatter(atl06["lon"].values, atl06["lat"].values, s=2.5, c=atl06["delta_time"], cmap='winter_r', zorder=3, transform=cartopy.crs.PlateCarree())
    #ax6.set_extent(extent,crs=cartopy.crs.PlateCarree())
    #ax6.plot(box_lon, box_lat, linewidth=1.5, color='r', zorder=2, transform=cartopy.crs.Geodetic())

    # Show Plot
    plt.show()