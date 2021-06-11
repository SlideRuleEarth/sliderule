#!/usr/bin/env python
u"""
atl06gs.py
"""
#
# Imports
#
import sys
import logging

import pandas as pd
import numpy as np
import math

from pyproj import Transformer, CRS
from shapely.geometry import Polygon, Point
import matplotlib.pyplot as plt
import cartopy

from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Default Parameters #
    error_threshold = 1.0
    url = ["127.0.0.1"]
    atl03_asset = "atlas-local"
    atl06_asset = "atlas-local"
    resource = "20210114170723_03311012_004_01.h5"
    region = [ {"lon": 126.54560629670780, "lat": -70.28232209449946},
               {"lon": 114.29798416287946, "lat": -70.08880029415151}, 
               {"lon": 112.05139144652648, "lat": -74.18128224472123},
               {"lon": 126.62732471857403, "lat": -74.37827832634999},
               {"lon": 126.54560629670780, "lat": -70.28232209449946} ]

    # Configure Logging #
    logging.basicConfig(level=logging.INFO)

    # Set URL #
    if len(sys.argv) > 1:
        url = sys.argv[1]
        atl03_asset = "atlas-s3"
        atl06_asset = "atlas-s3"

    # Bypass Service Discovery #
    if len(sys.argv) > 2:
        if sys.argv[2] == "bypass":
            url = [url]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Build ATL06 Request
    parms = { "poly": region,
              "cnf": 4,
              "ats": 20.0,
              "cnt": 10,
              "len": 40.0,
              "res": 20.0,
              "maxi": 1 }

    # Request ATL06 Data
    rsps = icesat2.atl06(parms, "ATL03_"+resource, atl03_asset, as_numpy=False)

    # Build DataFrame of SlideRule Response #
    sliderule = pd.DataFrame(rsps)

    # Display Status of SlideRule Processing #
    print("\nSliderule")
    print("---------")
    print("Reference Ground Tracks: {}".format(sliderule["rgt"].unique()))
    print("Cycles: {}".format(sliderule["cycle"].unique()))
    print("Received {} elevations".format(len(sliderule)))

    # Initialize Tracks to Read From #
    tracks = ["1l", "1r", "2l", "2r", "3l", "3r"]

    # Create Projection Transformer #
    transformer = Transformer.from_crs(4326, 3857) # GPS to Web Mercator

    # Project Polygon #
    pregion = []
    for point in region:
        ppoint = transformer.transform(point["lat"], point["lon"])
        pregion.append(ppoint)
    polygon = Polygon(pregion)

    # Build list of each lat,lon dataset to read
    geodatasets = [{"dataset": "/orbit_info/sc_orient"}]
    for track in tracks:
        prefix = "/gt"+track+"/land_ice_segments/"
        geodatasets.append({"dataset": prefix+"latitude", "startrow": 0, "numrows": -1})
        geodatasets.append({"dataset": prefix+"longitude", "startrow": 0, "numrows": -1})

    # Read lat,lon from resource
    geocoords = icesat2.h5p(geodatasets, "ATL06_"+resource, atl06_asset)

    # Build list of the subsetted h_li datasets to read
    hidatasets = []
    for track in tracks:
        prefix = "/gt"+track+"/land_ice_segments/"
        startrow = -1
        numrows = -1
        index = 0
        for index in range(len(geocoords[prefix+"latitude"])):
            lat = geocoords[prefix+"latitude"][index]
            lon = geocoords[prefix+"longitude"][index]
            c = transformer.transform(lat, lon)
            point = Point(c[0], c[1])
            intersect = point.within(polygon)
            if startrow == -1 and intersect:
                startrow = index
            elif startrow != -1 and not intersect:
                numrows = index - startrow
                break
        hidatasets.append({"dataset": prefix+"h_li", "startrow": startrow, "numrows": numrows, "prefix": prefix})
        hidatasets.append({"dataset": prefix+"segment_id", "startrow": startrow, "numrows": numrows, "prefix": prefix})

    # Read h_li from resource
    hivalues = icesat2.h5p(hidatasets, "ATL06_"+resource, atl06_asset)

    # Build Results #
    atl06 = {"h_mean": [], "lat": [], "lon": [], "segment_id": [], "spot": []}
    prefix2spot = { "/gt1l/land_ice_segments/": {0: 5, 1: 2},
                    "/gt1r/land_ice_segments/": {0: 6, 1: 1},
                    "/gt2l/land_ice_segments/": {0: 3, 1: 4},
                    "/gt2r/land_ice_segments/": {0: 4, 1: 3},
                    "/gt3l/land_ice_segments/": {0: 1, 1: 6},
                    "/gt3r/land_ice_segments/": {0: 2, 1: 5} }
    for entry in hidatasets:
        if "h_li" in entry["dataset"]:
            atl06["h_mean"] += hivalues[entry["prefix"]+"h_li"].tolist()
            atl06["lat"] += geocoords[entry["prefix"]+"latitude"][entry["startrow"]:entry["startrow"]+entry["numrows"]].tolist()
            atl06["lon"] += geocoords[entry["prefix"]+"longitude"][entry["startrow"]:entry["startrow"]+entry["numrows"]].tolist()
            atl06["segment_id"] += hivalues[entry["prefix"]+"segment_id"].tolist()
            atl06["spot"] += [prefix2spot[entry["prefix"]][geocoords["/orbit_info/sc_orient"][0]] for i in range(entry["numrows"])]

    # Build DataFrame of ATL06 NSIDC Data #
    nsidc = pd.DataFrame(atl06)

    # Display Status of SlideRule Processing #
    print("\nASAS")
    print("-----")
    print("Received {} elevations".format(len(nsidc)))

    # Initialize Error Variables #
    diff_set = ["h_mean", "lat", "lon"]
    errors = {}
    total_error = {}
    segments = {}    
    orphans = {"segment_id": [], "h_mean": [], "lat": [], "lon": []}

    # Create Segment Sets #
    for index, row in nsidc.iterrows():
        segment_id = row["segment_id"]
        # Create Difference Row for Segment ID #
        if segment_id not in segments:
            segments[segment_id] = {}
            for spot in [1, 2, 3, 4, 5, 6]:
                segments[segment_id][spot] = {}
                for process in ["sliderule", "nsidc", "difference"]:
                    segments[segment_id][spot][process] = {}
                    for element in diff_set:
                        segments[segment_id][spot][process][element] = 0.0                        
        for element in diff_set:
            segments[segment_id][row["spot"]]["nsidc"][element] = row[element]
    for index, row in sliderule.iterrows():
        segment_id = row["segment_id"]
        if segment_id not in segments:
            orphans["segment_id"].append(segment_id)
        else:
            for element in diff_set:
                segments[segment_id][row["spot"]]["sliderule"][element] = row[element]
                segments[segment_id][row["spot"]]["difference"][element] = segments[segment_id][row["spot"]]["sliderule"][element] - segments[segment_id][row["spot"]]["nsidc"][element]

    # Flatten Segment Sets to just Differences #
    for element in diff_set:
        errors[element] = []
        total_error[element] = 0.0
        for segment_id in segments:
            for spot in [1, 2, 3, 4, 5, 6]:
                error = segments[segment_id][spot]["difference"][element]
                if(abs(error) > error_threshold):
                    orphans[element].append(error)
                else:
                    errors[element].append(error)
                    total_error[element] += error

    # Display Status of Differencing #
    print("\nErrors")
    print("------")
    print("Estimated Segments Missing: {}".format(len(orphans["segment_id"]) / 6)) # one for each spot
    print("Height Errors: {} orphans, {:.5} total".format(len(orphans["h_mean"]), total_error["h_mean"]))
    print("Latitude Errors: {} orphans, {:.5} total".format(len(orphans["lat"]), total_error["lat"]))
    print("Longitude Errors: {} orphans, {:.5} total".format(len(orphans["lon"]), total_error["lon"]))

    # Create Plots
    fig = plt.figure(num=None, figsize=(12, 8))

    ax1 = plt.subplot(141)
    ax1.set_title("Height Errors")
    ax1.scatter([i for i in range(len(errors["h_mean"]))], errors["h_mean"], s=1.5, color='r')

    ax2 = plt.subplot(142)
    ax2.set_title("Latitude Errors")
    ax2.scatter([i for i in range(len(errors["lat"]))], errors["lat"], s=1.5, color='r')

    ax3 = plt.subplot(143)
    ax3.set_title("Longitude Errors")
    ax3.scatter([i for i in range(len(errors["lon"]))], errors["lon"], s=1.5, color='r')

    ax4 = plt.subplot(144,projection=cartopy.crs.PlateCarree())
    ax4.set_title("Track Plots")
    ax4.scatter(nsidc["lon"].values, nsidc["lat"].values, s=2.00, color='r', zorder=2, transform=cartopy.crs.PlateCarree())
    ax4.scatter(sliderule["lon"].values, sliderule["lat"].values, s=0.05, color='b', zorder=3, transform=cartopy.crs.PlateCarree())

    fig.tight_layout()
    plt.show()
