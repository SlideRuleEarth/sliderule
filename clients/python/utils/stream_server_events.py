#
# Connects to SlideRule server node at provided url and prints log messages
# as they are generated on server to local terminal.  
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

    # Override duration to maintain connection
    duration = 30 # seconds
    if len(sys.argv) > 2:
        duration = int(sys.argv[2])

    # Override event type
    event_type = "LOG"
    if len(sys.argv) > 3:
        event_type = sys.argv[3]

    # Override event level
    event_level = "INFO"
    if len(sys.argv) > 4:
        event_level = sys.argv[4]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, True)

    # Build Logging Request
    rqst = {
        "type": event_type, 
        "level" : event_level,
        "duration": duration
    }

    # Retrieve logs
    rsps = sliderule.source("event", rqst, stream=True)
