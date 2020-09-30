# python

import sys
import json
import sliderule

asset = "atl03-local"

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line

    if len(sys.argv) > 1:
        sliderule.set_url(sys.argv[1])

    # Override asset from command line

    if len(sys.argv) > 2:
        asset = sys.argv[2]

    # Build Index

    rqst = {
        "asset": asset,
        "resources": h5file,
        "dataset": "/gt1r/geolocation/segment_ph_cnt",
        "datatype": sliderule.datatypes["INTEGER"],
        "id": 0
    }

    d = sliderule.engine("h5", rqst)
    v = sliderule.get_values(d[0]["data"], d[0]["datatype"], d[0]["size"])