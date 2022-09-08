import sys
import json
import sliderule
from sliderule import icesat2

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = "127.0.0.1"
    organization=None

    # Server URL
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Organization
    if len(sys.argv) > 2:
        organization = sys.argv[2]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, organization=organization)

    # Query Version
    rsps = sliderule.source("version", {})

    # Display Version Information
    print(json.dumps(rsps, indent=4))