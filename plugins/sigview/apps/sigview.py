#
# Imports
#
import sys
import logging
import json

import pandas as pd
import numpy as np
import math

from sliderule import sliderule, icesat2

#
# SlideRule Processing Request
#
def atl00exec(parms):

    # Request ATL00 Processed Data
    rsps  = sliderule.engine("atl00", parms)

    return rsps

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # configure logging
    logging.basicConfig(level=logging.INFO)

    # Read Request #
    parms_filename = sys.argv[1]
    with open(parms_filename) as parmfile:
        parms = json.load(parmfile)

    # Set URL #
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 2:
        url = sys.argv[2]

    # Initialize Icesat2 Package #
    icesat2.init(url, True)

    # Execute SlideRule Algorithm
    rsps = atl00exec(parms)
    