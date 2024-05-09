"""Tests for sliderule gebco raster support."""

import pytest
from pathlib import Path
import sliderule

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

lon = -40
lat =  70

value = -64  # meters
file  = '/vsis3/sliderule/data/GEBCO/2023/cog_gebco_2023_sub_ice_n90.0_s0.0_w-90.0_e0.0.tif'

@pytest.mark.network
class TestGebco:
    def test_samples(self, init):
        rqst = {"samples": {"asset": "gebco-bathy"}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert rsps["samples"][0][0]["value"] == value
        assert rsps["samples"][0][0]["file"] ==  file
