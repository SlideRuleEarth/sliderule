"""Tests for sliderule opendata global canopy raster support."""

import pytest
from pathlib import Path
import sliderule

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -62.0
vrtLat =  -3.0

vrtValue     = 20
vrtFile      = '/vsis3/sliderule/data/GLOBALCANOPY/META_GlobalCanopyHeight_1m_2024_v1.vrt'
vrtFileTime  = 1396483218000

@pytest.mark.network
class TestGlobalCanopy:
    def test_samples(self, init):
        rqst = {"samples": {"asset": "meta-globalcanopy-1meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime
