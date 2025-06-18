"""Tests for sliderule icesat2 geojson support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

class TestGeoJson:
    def test_atl03(self, init):
        for testfile in ["data/grandmesa.geojson", "data/grandmesa.shp"]:
            region = sliderule.toregion(os.path.join(TESTDIR, testfile))
            parms = {
                "poly": region["poly"],
                "region_mask": region["raster"],
                "srt": icesat2.SRT_LAND,
                "cnf": icesat2.CNF_SURFACE_HIGH,
                "ats": 10.0,
                "cnt": 10,
                "len": 40.0,
                "res": 20.0,
            }
            gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_005_01.h5")
            assert init
            assert gdf["rgt"].unique()[0] == 295
            assert gdf["cycle"].unique()[0] == 1
            assert len(gdf) == 21006
