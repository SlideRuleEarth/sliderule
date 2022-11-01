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

    url = "127.0.0.1"
    attr = "SourceEndpoint"
    organization = None

    # Override server URL from command line
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Override organization
    if len(sys.argv) > 2:
        organization = sys.argv[2]

    # Override group of endpoints to query
    if len(sys.argv) > 3:
        attr = sys.argv[3]

    # Initialize ICESat2/SlideRule package
    icesat2.init(url, True, organization=organization)

    # Retrieve metrics
    rsps = sliderule.source("metric", { "attr": attr }, stream=False)

    # Display metrics
    print(json.dumps(rsps, indent=2))
