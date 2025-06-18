"""Tests for sliderule opendata global canopy raster support."""

import pytest
from pathlib import Path
import sliderule
from sliderule import raster

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -62.0
vrtLat =  -3.0

vrtValue     = 20
vrtFile      = '/vsis3/sliderule/data/GLOBALCANOPY/META_GlobalCanopyHeight_1m_2024_v1.vrt'
vrtFileTime  = 1396483218

class TestGlobalCanopy:
    def test_samples(self, init):
        rqst = {"samples": {"asset": "meta-globalcanopy-1meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime

    def test_sample_api_serial(self, init):
        gdf = raster.sample("meta-globalcanopy-1meter", [[vrtLon,vrtLat]])
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtValue) < sigma
        assert gdf["file"].iat[0] ==  vrtFile

    def test_sample_api_batch(self, init):
        gdf = raster.sample("meta-globalcanopy-1meter", [[vrtLon,vrtLat],[vrtLon+0.01,vrtLat+0.01]])
        assert init
        assert len(gdf) == 2
        assert abs(gdf["value"].iat[0] - vrtValue) < sigma
        assert gdf["file"].iat[0] ==  vrtFile

