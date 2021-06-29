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

    heights_32 = icesat2.h5("/gt1l/land_ice_segments/h_li", "ATL06_20181110092841_06530106_003_01.h5", atl06_asset)
    expected_32 = [350.76831055, 352.20120239, 352.45202637, 353.35467529, 353.70120239, 352.09786987, 349.70581055, 346.0881958, 342.42398071, 341.35415649]

    bckgrd_32nf = icesat2.h5("/gt1l/bckgrd_atlas/bckgrd_rate", "ATL03_20181016104402_02720106_003_01.h5", atl03_asset)
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

    v = icesat2.h5("/gt1r/geolocation/segment_ph_cnt", h5file, atl03_asset)
    if v[0] == 245 and v[1] == 263 and v[2] == 273:
        logging.info("Passed variable length test")
    else:
        logging.error("Failed variable length test: ", v)

#
#  TEST H5P
#
def test_h5p (atl06_asset):

    datasets = [
        {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
        {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
        {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
        {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
        {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
        {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5}
    ]

    rsps = icesat2.h5p(datasets, "ATL06_20181019065445_03150111_003_01.h5", atl06_asset)

    expected = {'/gt1l/land_ice_segments/h_li': [45.68811156, 45.71368944, 45.74234326, 45.74614113, 45.79866465],
                '/gt1r/land_ice_segments/h_li': [45.72632446, 45.76512574, 45.76337375, 45.77102473, 45.81307948],
                '/gt2r/land_ice_segments/h_li': [45.3146427 , 45.27640582, 45.23608027, 45.21131015, 45.15692304],
                '/gt2l/land_ice_segments/h_li': [45.35118977, 45.33535027, 45.27195617, 45.21816889, 45.18534204],
                '/gt3r/land_ice_segments/h_li': [45.14954134, 45.18970635, 45.16637644, 45.15235916, 45.17135806],
                '/gt3l/land_ice_segments/h_li': [45.29602321, 45.34764226, 45.31430979, 45.31471701, 45.30034622]}

    cmp_error = False
    last_fail = ""
    for dataset in expected.keys():
        for index in range(len(expected[dataset])):
            if round(rsps[dataset][index]) != round(expected[dataset][index]):
                print(dataset, index, rsps[dataset][index], expected[dataset][index])
                cmp_error = True
                last_fail = dataset

    if(not cmp_error):
        logging.info("Passed h5p test")
    else:
        logging.error("Failed h5p test: %s %s", last_fail, str(rsps[last_fail]))


#
#  TEST GEOSPATIAL
#
def test_geospatial (asset):

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
        "asset": atl03_asset,
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
        "asset": atl03_asset,
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
        "asset": atl03_asset,
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
#  TEST DEFINITION
#
def test_definition ():
    rqst = {
        "rectype": "atl03rec",
    }

    d = sliderule.source("definition", rqst)

    if(d["delta_time"]["offset"] == 576):
        logging.info("Passed definition test")
    else:
        logging.error("Failed definition test: {}".format(d["delta_time"]["offset"]))

#
#  TEST VERSION
#
def test_version ():

    rsps = sliderule.source("version", {})

    success = True
    success = success and ('server' in rsps)
    success = success and ('version' in rsps['server'])
    success = success and ('commit' in rsps['server'])
    success = success and ('launch' in rsps['server'])
    success = success and ('duration' in rsps['server'])
    success = success and ('packages' in rsps['server'])
    success = success and ('.' in rsps['server']['version'])
    success = success and ('v' in rsps['server']['commit'])
    success = success and ('-g' in rsps['server']['commit'])
    success = success and (':' in rsps['server']['launch'])
    success = success and (rsps['server']['duration'] > 0)
    success = success and ('icesat2' in rsps['server']['packages'])
    success = success and ('version' in rsps['icesat2'])
    success = success and ('commit' in rsps['icesat2'])
    success = success and ('.' in rsps['icesat2']['version'])
    success = success and ('v' in rsps['icesat2']['commit'])
    success = success and ('-g' in rsps['icesat2']['commit'])

    if success:
        logging.info("Passed version test")
    else:
        logging.error("Failed version test: {}".format(str(rsps)))


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    url = ["127.0.0.1"]
    atl03_asset = "atlas-local"
    atl06_asset = "atlas-local"

    # Override server URL from command line
    if len(sys.argv) > 1:
        url = sys.argv[1]
        atl03_asset = "atlas-s3"
        atl06_asset = "atlas-s3"

    # Override asset from command line
    if len(sys.argv) > 2:
        atl03_asset = sys.argv[2]

    # Override asset from command line
    if len(sys.argv) > 3:
        atl06_asset = sys.argv[3]

    # Bypass service discovery
    if len(sys.argv) > 4:
        if sys.argv[4] == "bypass":
            url = [url]

    # Initialize ICESat2/SlideRule Package
    icesat2.init(url, False)

    # Tests
    test_time()
    test_h5(atl03_asset)
    test_h5_types(atl03_asset, atl06_asset)
    test_variable_length(atl03_asset)
    test_h5p(atl06_asset)
    test_geospatial(atl03_asset)
    test_definition()
    test_version()
