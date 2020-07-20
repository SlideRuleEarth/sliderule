# python

import sys

sys.path.append("/usr/local/etc/sliderule")

import json
import sliderule

#h5file = "/home/user1/demo/ATL03_20181014040628_02370109_002_01.h5"
h5file = "/data/ATLAS/ATL03_20181014040628_02370109_002_01.h5"

###############################################################################
# TESTS
###############################################################################

#
#  TEST ECHO
#
def test_echo ():
    d = sliderule.echo('{ "hello" : "world" }')
    d = json.loads(d) # not sure why this is needed when sliderule.echo should return table
    if d["hello"] == "world":
        print("Passed echo test")
    else:
        print("Failed echo test")

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
        "filename": h5file,
        "dataset": "ancillary_data/atlas_sdp_gps_epoch",
        "datatype": sliderule.datatypes["REAL"],
        "id": 0
    }

    d = sliderule.engine("h5", rqst)
    v = sliderule.get_values(d[0]["DATA"], d[0]["DATATYPE"], d[0]["SIZE"])

    epoch_offset = v[0]
    if(epoch_offset == 1198800018.0):
        print("Passed h5 test")
    else:
        print("Failed h5 test")

#
#  TEST VARIABLE LENGTH
#
def test_variable_length ():
    rqst = {
        "filename": h5file,
        "dataset": "/gt1r/geolocation/segment_ph_cnt",
        "datatype": sliderule.datatypes["INTEGER"],
        "id": 0
    }

    d = sliderule.engine("h5", rqst)
    v = sliderule.get_values(d[0]["DATA"], d[0]["DATATYPE"], d[0]["SIZE"])

    if v[0] == 75 and v[1] == 82 and v[2] == 61:
        print("Passed variable length test")
    else:
        print("Failed variable length test")

#
#  TEST DEFINITION
#
def test_definition ():
    rqst = {
        "rectype": "atl03rec",
    }

    d = sliderule.source("definition", rqst)

    if(d["GPS"]["offset"] == 384):
        print("Passed definition test")
    else:
        print("Failed definition test", d["GPS"]["offset"])

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line

    if len(sys.argv) > 1:
        sliderule.set_url(sys.argv[1])

    # Populate record definitions

    sliderule.populate("atl03rec")
    sliderule.populate("atl03rec.photons")
    sliderule.populate("h5dataset")

    # Tests

    test_echo()
    test_time()
    test_h5()
    test_variable_length()
    test_definition()
