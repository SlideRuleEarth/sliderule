# python

import sys
import json
import logging
import sliderule
import icesat2

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
def test_h5 ():
    rqst = {
        "asset": asset,
        "resource": h5file,
        "dataset": "ancillary_data/atlas_sdp_gps_epoch",
        "datatype": sliderule.datatypes["REAL"],
        "id": 0
    }

    d = sliderule.engine("h5", rqst)
    v = icesat2.get_values(d[0]["data"], d[0]["datatype"], d[0]["size"])

    epoch_offset = v[0]
    if(epoch_offset == 1198800018.0):
        logging.info("Passed h5 test")
    else:
        logging.error("Failed h5 test: ", v)

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
    v = icesat2.get_values(d[0]["data"], d[0]["datatype"], d[0]["size"])

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

#
#  TEST GEOSPATIAL
#
def test_geospatial ():

    # Test 1 #

    test1 = {
        "asset": asset,
        "pole": "north",
        "lat": 40.0,
        "lon": 60.0,
        "x": 0.466307658155,
        "y": 0.80766855588292,
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

    d = sliderule.source("geo", test1)

    if(d["intersect"] == True):
        logging.info("Passed intersection test")
    else:
        logging.error("Failed intersection test", d["intersect"])

    if(abs(d["combine"]["lat0"] - 44.4015) < 0.001 and abs(d["combine"]["lon0"] - 108.6949) < 0.001 and\
       d["combine"]["lat1"] == 30.0 and d["combine"]["lon1"] == 100.0):
        logging.info("Passed combination test")
    else:
        logging.error("Failed combination test", d["combine"])

    if(abs(d["split"]["lspan"]["lat0"] - 18.6736) < 0.001 and abs(d["split"]["lspan"]["lon0"] - 106.0666) < .001 and\
       abs(d["split"]["lspan"]["lat1"] - 15.6558) < 0.001 and abs(d["split"]["lspan"]["lon1"] - 102.1886) < .001 and\
       abs(d["split"]["rspan"]["lat0"] - 19.4099) < 0.001 and abs(d["split"]["rspan"]["lon0"] - 103.0705) < .001 and\
       abs(d["split"]["rspan"]["lat1"] - 16.1804) < 0.001 and abs(d["split"]["rspan"]["lon1"] -  99.3163) < .001):
        logging.info("Passed split test")
    else:
        logging.error("Failed split test", d["split"])

    if(d["lat"] == 40.0 and d["lon"] == 60.0):
        logging.info("Passed sphere test")
    else:
        logging.error("Failed sphere test", d["lat"], d["lon"])

    if(d["x"] == 0.466307658155 and d["y"] == 0.80766855588292):
        logging.info("Passed projection test")
    else:
        logging.error("Failed projection test", d["x"], d["y"])

    # Test 2 # 

    test2 = {
        "asset": asset,
        "pole": "north",
        "lat": 30.0,
        "lon": 100.0,
        "x": -0.20051164424058,
        "y": 1.1371580426033,
    }

    d = sliderule.source("geo", test2)

    if(abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == 100.0):
        logging.info("Passed sphere2 test")
    else:
        logging.error("Failed sphere2 test", d["lat"], d["lon"])

    # Test 3 # 

    test3 = {
        "asset": asset,
        "pole": "north",
        "lat": 30.0,
        "lon": 100.0,
        "x": -0.20051164424058,
        "y": -1.1371580426033,
    }

    d = sliderule.source("geo", test3)

    if(abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == -100.0):
        logging.info("Passed sphere3 test")
    else:
        logging.error("Failed sphere3 test", d["lat"], d["lon"])

    # Test 4 # 

    test4 = {
        "asset": asset,
        "pole": "north",
        "lat": 30.0,
        "lon": 100.0,
        "x": 0.20051164424058,
        "y": -1.1371580426033,
    }

    d = sliderule.source("geo", test4)

    if(abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == -80.0):
        logging.info("Passed sphere4 test")
    else:
        logging.error("Failed sphere4 test", d["lat"], d["lon"])

#
#  TEST INDEX
#
def test_index ():
    test1 = {
        "rgtindex": {"rgt": 295},
        "timeindex": {"t0": 1239544000, "t1": 1255238200}
    }

    d = sliderule.source("index", test1)

    if(len(d["resources"]) == 20):
        logging.info("Passed union index test")
    else:
        logging.error("Failed union index test", len(d["resources"]), d)

    test2 = {
        "and": {
            "rgtindex": {"rgt": 295},
            "timeindex": {"t0": 1239544000, "t1": 1255238200}
        }
    }

    d = sliderule.source("index", test2)

    if( ("ATL03_20190417134754_02950302_003_01.h5" in d["resources"]) and 
        ("ATL03_20191016050727_02950502_003_01.h5" in d["resources"]) and 
        (len(d["resources"]) == 2)):
        logging.info("Passed intersection index test")
    else:
        logging.error("Failed intersection index test", len(d["resources"]), d)

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line
    url = "http://127.0.0.1:9081"
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Override asset from command line
    asset = "atl03-local"
    if len(sys.argv) > 2:
        asset = sys.argv[2]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, False)

    # Tests
    test_time()
    test_h5()
    test_variable_length()
    test_definition()
    test_geospatial()
    test_index()
