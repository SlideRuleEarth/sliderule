"""Tests for sliderule rema raster support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -108.1
vrtLat =   39.1

vrtValue     = 20
vrtFile      = '/vsis3/esa-worldcover/v100/2020/ESA_WorldCover_10m_2020_v100_Map_AWS.vrt'
vrtFileTime  = 1309046423000.0

@pytest.mark.network
class TestMosaic:
    def test_vrt(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        rqst = {"samples": {"asset": "esa-worldcover-10meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert abs(rsps["samples"][0][0]["value"] - vrtValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime
