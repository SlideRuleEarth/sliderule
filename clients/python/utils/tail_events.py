#
# Connects to SlideRule server node at provided url and prints the last
# 1K log messages to local terminal.
#
# This bypasses service discovery and goes directly to the server node.
#
# Use query_services.py to get list of server node IP addresses
#
import sys
import logging
from sliderule import sliderule
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
        "monitor":      'EventMonitor'
    }

    # Parse Configuration Parameters
    parse_command_line(sys.argv, cfg)

    # configure logging
    logging.basicConfig(level=logging.INFO)

    # Initialize ICESat2/SlideRule Package
    icesat2.init(cfg["url"], True, organization=cfg["organization"])

    # Build Logging Request
    rqst = {
        "monitor": cfg["monitor"]
    }

    # Retrieve logs
    rsps = sliderule.source("tail", rqst, stream=False)

    # Display logs
    for rsp in rsps:
        print(rsp, end='')
