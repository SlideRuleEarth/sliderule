"""Tests for sliderule merit-dem raster support."""

import pytest
from sliderule import sliderule, raster

class TestMerit:
    def test_sample(self, init):
        sliderule.set_rqst_timeout((10,600))
        gdf = raster.sample("merit-dem", [[-172, 51.7], [-172, 51.71], [-172, 51.72], [-172, 51.73], [-172, 51.74]])
        assert init
        assert gdf["value"].iloc[0] == -99990000
        assert len(gdf) == 5
