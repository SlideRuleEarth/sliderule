"""Tests for sliderule-python icesat2 api."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule
from sliderule import icesat2
from pathlib import Path
import os.path

SERVER = "icesat2sliderule.org"
ASSET = "nsidc-s3"

ATL03_FILE1 = "ATL03_20181019065445_03150111_004_01.h5"
ATL03_FILE2 = "ATL03_20181016104402_02720106_004_01.h5"
ATL06_FILE1 = "ATL06_20181019065445_03150111_004_01.h5"
ATL06_FILE2 = "ATL06_20181110092841_06530106_004_01.h5"

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

@pytest.mark.network
class TestApi:
    def test_time(self):
        icesat2.init(SERVER)
        rqst = {
            "time": "NOW",
            "input": "NOW",
            "output": "GPS" }
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
        assert now == again

    def test_h5(self):
        icesat2.init(SERVER)
        epoch_offset = icesat2.h5("ancillary_data/atlas_sdp_gps_epoch", ATL03_FILE1, ASSET)[0]
        assert epoch_offset == 1198800018.0

    def test_h5_types(self):
        icesat2.init(SERVER)
        heights_64 = icesat2.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE1, ASSET)
        expected_64 = [45.95665, 45.999374, 46.017857, 46.015575, 46.067562, 46.099796, 46.14037, 46.105526, 46.096024, 46.12297]
        heights_32 = icesat2.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE2, ASSET)
        expected_32 = [350.46988, 352.08688, 352.43243, 353.19345, 353.69543, 352.25998, 350.15366, 346.37888, 342.47903, 341.51]
        bckgrd_32nf = icesat2.h5("/gt1l/bckgrd_atlas/bckgrd_rate", ATL03_FILE2, ASSET)
        expected_32nf = [29311.684, 6385.937, 6380.8413, 28678.951, 55349.168, 38201.082, 19083.434, 38045.67, 34942.434, 38096.266]
        for c in zip(heights_64, expected_64, heights_32, expected_32, bckgrd_32nf, expected_32nf):
            assert (round(c[0]) == round(c[1])) and (round(c[2]) == round(c[3])) and (round(c[4]) == round(c[5]))

    def test_variable_length(self):
        icesat2.init(SERVER)
        v = icesat2.h5("/gt1r/geolocation/segment_ph_cnt", ATL03_FILE1, ASSET)
        assert v[0] == 258 and v[1] == 256 and v[2] == 273

    def test_h5p(self):
        icesat2.init(SERVER)
        datasets = [
            {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = icesat2.h5p(datasets, ATL06_FILE1, ASSET)
        expected = {'/gt1l/land_ice_segments/h_li': [45.95665, 45.999374, 46.017857, 46.015575, 46.067562],
                    '/gt1r/land_ice_segments/h_li': [45.980865, 46.02602, 46.02262, 46.03137, 46.073578],
                    '/gt2l/land_ice_segments/h_li': [45.611526, 45.588196, 45.53242, 45.48105, 45.443752],
                    '/gt2r/land_ice_segments/h_li': [45.547, 45.515495, 45.470577, 45.468964, 45.406998],
                    '/gt3l/land_ice_segments/h_li': [45.560867, 45.611183, 45.58064, 45.579746, 45.563858],
                    '/gt3r/land_ice_segments/h_li': [45.39587, 45.43603, 45.412586, 45.40014, 45.41833]}
        for dataset in expected.keys():
            for index in range(len(expected[dataset])):
                assert round(rsps[dataset][index]) == round(expected[dataset][index])

    def test_geospatial1(self):
        icesat2.init(SERVER)
        test = {
            "asset": ASSET,
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
            }
        }
        d = sliderule.source("geo", test)
        assert d["intersect"] == True
        assert abs(d["combine"]["lat0"] - 44.4015)  < 0.001
        assert abs(d["combine"]["lon0"] - 108.6949) < 0.001
        assert d["combine"]["lat1"] == 30.0
        assert d["combine"]["lon1"] == 100.0
        assert abs(d["split"]["lspan"]["lat0"] - 18.6736)  < 0.001
        assert abs(d["split"]["lspan"]["lon0"] - 106.0666) < 0.001
        assert abs(d["split"]["lspan"]["lat1"] - 15.6558)  < 0.001
        assert abs(d["split"]["lspan"]["lon1"] - 102.1886) < 0.001
        assert abs(d["split"]["rspan"]["lat0"] - 19.4099)  < 0.001
        assert abs(d["split"]["rspan"]["lon0"] - 103.0705) < 0.001
        assert abs(d["split"]["rspan"]["lat1"] - 16.1804)  < 0.001
        assert abs(d["split"]["rspan"]["lon1"] - 99.3163)  < 0.001
        assert d["lat"] == 40.0 and d["lon"] == 60.0
        assert d["x"] == 0.466307658155 and d["y"] == 0.80766855588292

    def test_geospatial2(self):
        icesat2.init(SERVER)
        test = {
            "asset": ASSET,
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": -0.20051164424058,
            "y": 1.1371580426033,
        }
        d = sliderule.source("geo", test)
        assert abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == 100.0

    def test_geospatial3(self):
        icesat2.init(SERVER)
        test = {
            "asset": ASSET,
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": -0.20051164424058,
            "y": -1.1371580426033,
        }
        d = sliderule.source("geo", test)
        assert abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == -100.0

    def test_geospatial4(self):
        icesat2.init(SERVER)
        test = {
            "asset": ASSET,
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": 0.20051164424058,
            "y": -1.1371580426033,
        }
        d = sliderule.source("geo", test)
        assert abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == -80.0

    def test_definition(self):
        icesat2.init(SERVER)
        rqst = {
            "rectype": "atl06rec.elevation",
        }
        d = sliderule.source("definition", rqst)
        assert d["delta_time"]["offset"] == 192

    def test_version(self):
        icesat2.init(SERVER)
        rsps = sliderule.source("version", {})
        assert 'server' in rsps
        assert 'version' in rsps['server']
        assert 'commit' in rsps['server']
        assert 'launch' in rsps['server']
        assert 'duration' in rsps['server']
        assert 'packages' in rsps['server']
        assert '.' in rsps['server']['version']
        assert '-g' in rsps['server']['commit']
        assert ':' in rsps['server']['launch']
        assert rsps['server']['duration'] > 0
        assert 'icesat2' in rsps['server']['packages']
        assert 'version' in rsps['icesat2']
        assert 'commit' in rsps['icesat2']
        assert '.' in rsps['icesat2']['version']
        assert '-g' in rsps['icesat2']['commit']
