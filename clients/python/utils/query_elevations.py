#
# Perform a proxy request for atl06-sr elevations
#

import sys
import logging
import time
from sliderule import icesat2
from utils import initialize_client

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Configure Logging
    logging.basicConfig(level=logging.INFO)

    # Initialize Client
    parms, cfg = initialize_client(sys.argv)

    # Request ATL06 Data
    tstart = time.perf_counter()
    gdf = icesat2.atl06p(parms, asset=cfg["asset"], resources=[cfg["resource"]])
    perf_duration = time.perf_counter() - tstart

    # Display Statistics
    print("Completed in {:.3f} seconds of wall-clock time".format(perf_duration))
    if len(gdf) > 0:
        print("Reference Ground Tracks: {}".format(gdf["rgt"].unique()))
        print("Cycles: {}".format(gdf["cycle"].unique()))
        print("Received {} elevations".format(len(gdf)))
    else:
        print("No elevations were returned")

    # Display Profile
    print("\nTiming Profiles")
    for key in icesat2.profiles:
        print("{:16}: {:.6f} secs".format(key, icesat2.profiles[key]))