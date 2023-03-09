#
# Perform a proxy request for atl06-sr elevations
#

import sys
import logging
from sliderule import icesat2
from utils import initialize_client, display_statistics

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Configure Logging
    logging.basicConfig(level=logging.INFO)

    # Initialize Client
    parms, cfg = initialize_client(sys.argv)

    # Request ATL06 Data
    gdf = icesat2.atl06p(parms, asset=cfg["asset"], resources=[cfg["resource"]])

    # Display Statistics
    display_statistics(gdf, "elevations")
