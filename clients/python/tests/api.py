# python

import sys
import json
import logging
import sliderule
from sliderule import icesat2
from sliderule import datatypes

###############################################################################
# GLOBAL CODE
###############################################################################

# set resource
h5file = "ATL03_20181019065445_03150111_003_01.h5"

# configure logging
logging.basicConfig(level=logging.INFO)

###############################################################################
# TESTS
###############################################################################

#
#  TEST TIME
#
def test_time ():
    rqst = {
        "time": "NOW",
        "input": "NOW",
        "output": "GPS"
    }

    d = sliderule.source("time", rqst)

    now = d["time"] - (d["time"] % 1000) # gmt is in resolution of seconds, not milliseconds
    rqst["time"] = d["time"]
    rqst["input"] = "GPS"
    rqst["output"] = "GMT"

    d = sliderule.source("time", rqst)

    rqst["time"] = d["time"]
    rqst["input"] = "GMT"
    rqst["output"] = "GPS"

    d = sliderule.source("time", rqst)

    again = d["time"]

    if now == again:
        logging.info("Passed time test")
    else:
        logging.error("Failed time test")

#
#  TEST H5
#
def test_h5 (atl03_asset):

    epoch_offset = icesat2.h5("ancillary_data/atlas_sdp_gps_epoch", h5file, atl03_asset)[0]
    if(epoch_offset == 1198800018.0):
        logging.info("Passed h5 test")
    else:
        logging.error("Failed h5 test: ", v)

#
#  TEST H5 TYPES
#
def test_h5_types (atl03_asset, atl06_asset):

    heights_64 = icesat2.h5("/gt1l/land_ice_segments/h_li", "ATL06_20181019065445_03150111_003_01.h5", atl06_asset)
    expected_64 = [45.68811156, 45.71368944, 45.74234326, 45.74614113, 45.79866465, 45.82339277, 45.85106103, 45.81983169, 45.81150041, 45.83502945]

    heights_32 = icesat2.h5("/gt1l/land_ice_segments/h_li", "ATL06_20181110092841_06530106_003_01.h5", atl06_asset, "FLOAT")
    expected_32 = [350.76831055, 352.20120239, 352.45202637, 353.35467529, 353.70120239, 352.09786987, 349.70581055, 346.0881958, 342.42398071, 341.35415649]

    bckgrd_32nf = icesat2.h5("/gt1l/bckgrd_atlas/bckgrd_rate", "ATL03_20181016104402_02720106_003_01.h5", atl03_asset, "FLOAT")
    expected_32nf = [29311.68359375, 6385.93652344, 6380.84130859, 28678.95117188, 55349.19921875, 38201.08203125, 19083.43554688, 38045.66796875, 34942.43359375, 38096.26953125]

    cmp_error = False
    last_fail = (0,0,0,0)
    for c in zip(heights_64, expected_64, heights_32, expected_32, bckgrd_32nf, expected_32nf):
        if (round(c[0]) != round(c[1])) or (round(c[2]) != round(c[3])) or (round(c[4]) != round(c[5])):
            cmp_error = True
            last_fail = c
    
    if(not cmp_error):
        logging.info("Passed h5 types test")
    else:
        logging.error("Failed h5 types test: %s", str(last_fail))

#
#  TEST VARIABLE LENGTH
#
def test_variable_length (atl03_asset):

    v = icesat2.h5("/gt1r/geolocation/segment_ph_cnt", h5file, atl03_asset, datatype=datatypes["INTEGER"])
    if v[0] == 245 and v[1] == 263 and v[2] == 273:
        logging.info("Passed variable length test")
    else:
        logging.error("Failed variable length test: ", v)

#
#  TEST DEFINITION
#
def test_definition ():
    rqst = {
        "rectype": "atl03rec",
    }

    d = sliderule.source("definition", rqst)

    if(d["delta_time"]["offset"] == 448):
        logging.info("Passed definition test")
    else:
        logging.error("Failed definition test", d["delta_time"]["offset"])

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line
    url = ["127.0.0.1"]
    if len(sys.argv) > 1:
        url = [sys.argv[1]]

    # Override asset from command line
    atl03_asset = "atlas-local"
    if len(sys.argv) > 2:
        atl03_asset = sys.argv[2]

    # Override asset from command line
    atl06_asset = "atl06-local"
    if len(sys.argv) > 3:
        atl06_asset = sys.argv[3]

    # Check for use of service discovery
    if len(sys.argv) > 4:
        if sys.argv[4] == "service":
            url = url[0]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, False)

    # Tests
    test_time()
    test_h5(atl03_asset)
    test_h5_types(atl03_asset, atl06_asset)
    test_variable_length(atl03_asset)
    test_definition()
