#
# Connects to SlideRule server node at provided url and prints the metrics
# associated with the queried attribute.
#
# This bypasses service discovery and goes directly to the server node.
#
# Use query_services.py to get list of server node IP addresses
#
import sys
import logging
import json
from sliderule import sliderule
from sliderule import icesat2

###############################################################################
# GLOBAL CODE
###############################################################################

# configure logging
logging.basicConfig(level=logging.INFO)

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line
    url = ["127.0.0.1"]
    if len(sys.argv) > 1:
        url = [sys.argv[1]]

    # Override monitor name
    attr = "LuaEndpoint"
    if len(sys.argv) > 2:
        attr = sys.argv[2]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, True)

    # Build Metric Request
    rqst = {
        "attr": attr
    }

    # Retrieve Metrics
    rsps = sliderule.source("metric", rqst, stream=False)

    # Display Metrics
    print(json.dumps(rsps, indent=2))
