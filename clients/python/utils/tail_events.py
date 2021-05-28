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
    monitor = "EventMonitor"
    if len(sys.argv) > 2:
        monitor = int(sys.argv[2])

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, True)

    # Build Logging Request
    rqst = {
        "monitor": monitor 
    }

    # Retrieve logs
    rsps = sliderule.source("tail", rqst, stream=False)

    # Display logs
    for rsp in rsps:
        print(rsp, end='')
