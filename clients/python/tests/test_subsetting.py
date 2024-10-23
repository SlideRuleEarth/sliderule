"""Tests for sliderule subsetting code."""

import pytest
from sliderule import icesat2, sliderule
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestSubsetting:

    def test_rasterize(self, init):
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region['poly'],
            "region_mask": region['raster'],
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 953
        assert abs(gdf["h_mean"].describe()["mean"] - 1749.8443895024502) < 0.1
        assert abs(gdf["y_atc"].describe()["mean"] - -5516.94775390625) < 0.1

    def test_180_edge(self, init):
        resource = "ATL03_20221012073759_03291712_005_01.h5"
        poly = [ { "lat": -73.0, "lon": 155.00 },
                 { "lat": -74.0, "lon": 155.00 },
                 { "lat": -74.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": 155.00 } ]
        parms = {
            "poly": poly,
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 33998
        assert abs(gdf["h_mean"].describe()["mean"] - 1963.3553175453283) < 0.01
        assert abs(gdf["y_atc"].describe()["mean"] - 1.438330888748169) < 0.01

    def test_180_edge_plate_carree(self, init):
        resource = "ATL03_20221012073759_03291712_005_01.h5"
        poly = [ { "lat": -73.0, "lon": -180.00 },
                 { "lat": -74.0, "lon": -180.00 },
                 { "lat": -74.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": -180.00 } ]
        parms = {
            "poly": poly,
            "proj": "plate_carree",
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 33973
        assert abs(gdf["h_mean"].describe()["mean"] - 2105.1968807146873) < 0.01
        assert abs(gdf["y_atc"].describe()["mean"] - -0.3240036070346832) < 0.01

    def test_150_translation(self, init):
        resource = "ATL03_20221009072040_02831712_005_01.h5"
        poly = [ { "lat": -73.0, "lon": 160.00 },
                 { "lat": -74.0, "lon": 160.00 },
                 { "lat": -74.0, "lon": 170.00 },
                 { "lat": -73.0, "lon": 170.00 },
                 { "lat": -73.0, "lon": 160.00 } ]
        parms = {
            "poly": poly,
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 26663
        assert abs(gdf["h_mean"].describe()["mean"] - 95.01306951414814) < 0.01
        assert abs(gdf["y_atc"].describe()["mean"] - 77.65707397460938) < 0.01
