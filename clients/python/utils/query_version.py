import sys
import json
import logging
import sliderule
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

    # Query Version
    rsps = sliderule.source("version", {})

    # Display Version Information
    print(json.dumps(rsps, indent=4))