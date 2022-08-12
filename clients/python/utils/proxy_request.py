#
# Perform a proxy request
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
    asset = "nsidc-s3"

    logging.basicConfig(level=logging.INFO)

    # Region of Interest #
    region_filename = sys.argv[1]
    region = icesat2.toregion(region_filename)

    # Set URL #
    if len(sys.argv) > 2:
        url = sys.argv[2]

    # Set Asset #
    if len(sys.argv) > 3:
        asset = sys.argv[3]

    # Configure SlideRule #
    icesat2.init(url, True)

    # Build ATL06 Request #
    parms = {
        "poly": region["poly"],
        "raster": region["raster"],
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "atl08_class": ["atl08_ground"],
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
        "maxi": 1
    }

    # Get Granules
    resources = icesat2.cmr(polygon=region["poly"])

    # Latch Start Time
    perf_start = time.perf_counter()

    # Request ATL06 Data
    atl06 = icesat2.atl06p(parms, asset, resources=resources[0:1])

    # Latch Stop Time
    perf_stop = time.perf_counter()

    # Build DataFrame of SlideRule Responses
    num_elevations = len(atl06)

    # Display Statistics
    perf_duration = perf_stop - perf_start
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    if num_elevations > 0:
        print("Reference Ground Tracks: {}".format(atl06["rgt"].unique()))
        print("Cycles: {}".format(atl06["cycle"].unique()))
        print("Received {} elevations".format(num_elevations))
    else:
        print("No elevations were returned")

