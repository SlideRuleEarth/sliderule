import sys
import json
import logging
import sliderule
from sliderule import icesat2
from utils import parse_command_line

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Set Script Defaults
    cfg = {
        "url":          'localhost',
        "organization": None,
    }

    # Parse Configuration Parameters
    parse_command_line(sys.argv, cfg)

    # Configure Logging
    logging.basicConfig(level=logging.INFO)

    # Initialize ICESat2/SlideRule Package
    icesat2.init(cfg["url"], organization=cfg["organization"])

    # Query Version
    rsps = sliderule.source("version", {})

    # Display Version Information
    print(json.dumps(rsps, indent=4))