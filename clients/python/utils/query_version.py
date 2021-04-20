import sys
import json
import logging
import sliderule
from sliderule import icesat2
from sliderule import datatypes

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = ["127.0.0.1"]

    # Override server URL from command line
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Bypass service discovery
    if len(sys.argv) > 2:
        if sys.argv[2] == "bypass":
            url = [url]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, False)

    # Query Version
    rsps = sliderule.source("version", {})

    # Display Version Information
    print(json.dumps(rsps, indent=4))