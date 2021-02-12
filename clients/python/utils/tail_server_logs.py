#
# Connects to SlideRule server at provided url and prints log messages
# generated on server to local terminal 
#

import sys
import logging
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
    duration = 3 # seconds
    if len(sys.argv) > 2:
        duration = int(sys.argv[2])

    # Override log level
    lvl = "INFO"
    if len(sys.argv) > 3:
        lvl = sys.argv[3]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, True)

    # Retrieve logs
    icesat2.log(lvl, duration)
