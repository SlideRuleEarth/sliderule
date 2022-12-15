"""Tests for sliderule-python arcticdem raster support."""

import pytest
import sliderule
from sliderule import icesat2

@pytest.mark.network
class TestVrt:
    def test_vrt(self, domain, organization):
        icesat2.init(domain, organization=organization)
        rqst = {"dem-asset": "arcticdem-mosaic", "coordinates": [[-178.0,51.7]]}
        rsps = sliderule.source("samples", rqst)
        assert abs(rsps["samples"][0][0]["value"] - 80.713500976562) < 0.001
        assert rsps["samples"][0][0]["file"] == '/vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/70_09/70_09_2_1_2m_v3.0_reg_dem.tif'
