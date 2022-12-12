"""Tests for sliderule icesat2 geojson support."""

import pytest
from pathlib import Path
import os.path
from sliderule import icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestGeoJson:
    def test_atl06(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        for testfile in ["data/grandmesa.geojson", "data/grandmesa.shp"]:
            region = icesat2.toregion(os.path.join(TESTDIR, testfile))
            parms = {
                "poly": region["poly"],
                "raster": region["raster"],
                "srt": icesat2.SRT_LAND,
                "cnf": icesat2.CNF_SURFACE_HIGH,
                "ats": 10.0,
                "cnt": 10,
                "len": 40.0,
                "res": 20.0,
            }
            gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_005_01.h5", asset)
            assert gdf["rgt"].unique()[0] == 295
            assert gdf["cycle"].unique()[0] == 1
            assert len(gdf) == 21029
