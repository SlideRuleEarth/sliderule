"""Tests for sliderule opendata Copernicus 30 meter raster support."""

import pytest
from pathlib import Path
import sliderule
from sliderule import raster

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -108.1
vrtLat = 39.1

vrtValue = 2637.891093719017
vrtFile = "/vsis3/raster/COP30/COP30_hh.vrt"
vrtFileTime = 1386633618


class TestCopernicus30meter:
    def test_samples(self, init):
        rqst = {"samples": {"asset": "esa-copernicus-30meter"}, "coordinates": [[vrtLon, vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtValue) < sigma
        assert rsps["samples"][0][0]["file"] == vrtFile
        assert rsps["samples"][0][0]["time"] == vrtFileTime

    def test_sample_api_serial(self, init):
        gdf = raster.sample("esa-copernicus-30meter", [[vrtLon, vrtLat]])
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtValue) < sigma
        assert gdf["file"].iat[0] == vrtFile

    def test_sample_api_batch(self, init):
        gdf = raster.sample("esa-copernicus-30meter", [[vrtLon, vrtLat], [vrtLon + 0.01, vrtLat + 0.01]])
        assert init
        assert len(gdf) == 2
        assert abs(gdf["value"].iat[0] - vrtValue) < sigma
        assert gdf["file"].iat[0] == vrtFile
