"""Tests for sliderule opendata worldcover raster support."""

import pytest
from pathlib import Path
import sliderule

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -108.1
vrtLat =   39.1

vrtValue     = 10
vrtFile      = '/vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt'
vrtFileTime  = 1309046418000

@pytest.mark.network
class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "esa-worldcover-10meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime
