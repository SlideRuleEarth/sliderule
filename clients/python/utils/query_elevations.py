#
# Perform a proxy request for atl06-sr elevations
#

import sys
import logging
import time
from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = "127.0.0.1"
    asset = "atlas-local"
    organization = None
    resource = "ATL03_20181017222812_02950102_005_01.h5"

    logging.basicConfig(level=logging.INFO)

    # Region of Interest #
    region_filename = sys.argv[1]
    region = icesat2.toregion(region_filename)

    # Set URL #
    if len(sys.argv) > 2:
        url = sys.argv[2]
        asset = "nsidc-s3"

    # Set Asset #
    if len(sys.argv) > 3:
        asset = sys.argv[3]

    # Set Organization #
    if len(sys.argv) > 4:
        organization = sys.argv[4]

    # Set Resource #
    if len(sys.argv) > 5:
        organization = sys.argv[5]

    # Configure SlideRule #
    icesat2.init(url, True, organization=organization)

    # Build ATL06 Request #
    parms = {
        "poly": region["poly"],
        "raster": region["raster"],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # Request ATL06 Data
    perf_start = time.perf_counter()
    gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
    perf_stop = time.perf_counter()

    # Display Statistics
    num_elevations = len(gdf)
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    if num_elevations > 0:
        print("Reference Ground Tracks: {}".format(gdf["rgt"].unique()))
        print("Cycles: {}".format(gdf["cycle"].unique()))
        print("Received {} elevations".format(num_elevations))
    else:
        print("No elevations were returned")

