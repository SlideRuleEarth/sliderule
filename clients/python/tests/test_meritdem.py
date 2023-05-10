"""Tests for sliderule-python merit-dem raster support."""

import pytest
from pathlib import Path
from sliderule import raster
import sliderule

@pytest.mark.network
class TestMerit:
    def test_sample(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        gdf = raster.sample("merit-dem", [[-178, 51.7]])
        print(gdf)
#        assert abs(rsps["samples"][0][0]["value"] - 80.713500976562) < 0.001
#        assert rsps["samples"][0][0]["file"] == '/vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/70_09/70_09_2_1_2m_v3.0_reg_dem.tif'

