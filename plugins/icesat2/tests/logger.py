# python

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
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, True)

    # Test
    icesat2.log("INFO", 3)
