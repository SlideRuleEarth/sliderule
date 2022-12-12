"""Tests for sliderule-python icesat2 api."""

import pytest
import sliderule
from sliderule import icesat2

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

@pytest.mark.network
class TestApi:
    def test_time(self, domain, organization):
        icesat2.init(domain, organization=organization)
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

    def test_geospatial1(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        test = {
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

    def test_geospatial2(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        test = {
            "asset": asset,
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": -0.20051164424058,
            "y": 1.1371580426033,
        }
        d = sliderule.source("geo", test)
        assert abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == 100.0

    def test_geospatial3(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        test = {
            "asset": asset,
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": -0.20051164424058,
            "y": -1.1371580426033,
        }
        d = sliderule.source("geo", test)
        assert abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == -100.0

    def test_geospatial4(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        test = {
            "asset": asset,
            "pole": "north",
            "lat": 30.0,
            "lon": 100.0,
            "x": 0.20051164424058,
            "y": -1.1371580426033,
        }
        d = sliderule.source("geo", test)
        assert abs(d["lat"] - 30.0) < 0.0001 and d["lon"] == -80.0

    def test_definition(self, domain, organization):
        icesat2.init(domain, organization=organization)
        rqst = {
            "rectype": "atl06rec.elevation",
        }
        d = sliderule.source("definition", rqst)
        assert d["delta_time"]["offset"] == 256

    def test_version(self, domain, organization):
        icesat2.init(domain, organization=organization)
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
