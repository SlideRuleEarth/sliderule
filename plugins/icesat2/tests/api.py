# python

import sys
import json
import sliderule

asset = "atl03-local"
h5file = "ATL03_20181019065445_03150111_003_01.h5"

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
        print("Passed time test")
    else:
        print("Failed time test")

#
#  TEST H5
#
def test_h5 ():
    rqst = {
        "asset": asset,
        "resource": h5file,
        "dataset": "ancillary_data/atlas_sdp_gps_epoch",
        "datatype": sliderule.datatypes["REAL"],
        "id": 0
    }

    d = sliderule.engine("h5", rqst)
    v = sliderule.get_values(d[0]["data"], d[0]["datatype"], d[0]["size"])

    epoch_offset = v[0]
    if(epoch_offset == 1198800018.0):
        print("Passed h5 test")
    else:
        print("Failed h5 test: ", v)

#
#  TEST VARIABLE LENGTH
#
def test_variable_length ():
    rqst = {
        "asset": asset,
        "resource": h5file,
        "dataset": "/gt1r/geolocation/segment_ph_cnt",
        "datatype": sliderule.datatypes["INTEGER"],
        "id": 0
    }

    d = sliderule.engine("h5", rqst)
    v = sliderule.get_values(d[0]["data"], d[0]["datatype"], d[0]["size"])

    if v[0] == 245 and v[1] == 263 and v[2] == 273:
        print("Passed variable length test")
    else:
        print("Failed variable length test: ", v)

#
#  TEST DEFINITION
#
def test_definition ():
    rqst = {
        "rectype": "atl03rec",
    }

    d = sliderule.source("definition", rqst)

    if(d["gps"]["offset"] == 448):
        print("Passed definition test")
    else:
        print("Failed definition test", d["gps"]["offset"])

#
#  TEST GEOSPATIAL
#
def test_geospatial ():
    rqst = {
        "asset": asset,
        "pole": "south",
        "lat": 40.0,
        "lon": 60.0,
        "x": 2.3,
        "y": 4.9,
        "span": {
            "lat0": 20.0,
            "lon0": 100.0,
            "lat1": 15.0,
            "lon1": 105.0
        },
        "span1": {
            "lat0": 30.0,
            "lon0": 100.0,
            "lat1": 35.0,
            "lon1": 105.0
        },
        "span2": {
            "lat0": 32.0,
            "lon0": 101.0,
            "lat1": 45.0,
            "lon1": 106.0
        },
    }

    d = sliderule.source("geo", rqst)

    print("intersect:", d["intersect"])
    print("combine:", d["combine"])
    print("split:", d["split"])
    print("lat:", d["lat"])
    print("lon:", d["lon"])
    print("x:", d["x"])
    print("y:", d["y"])

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

    # Tests

    test_time()
    test_h5()
    test_variable_length()
    test_definition()
    test_geospatial()
