"""Tests for sliderule merit-dem raster support."""

import pytest
from pathlib import Path
from sliderule import raster
import sliderule

@pytest.mark.network
class TestMerit:
    def test_sample(self, init):
        gdf = raster.sample("merit-dem", [[-172, 51.7], [-172, 51.71], [-172, 51.72], [-172, 51.73], [-172, 51.74]])
        assert init
        assert gdf["value"][0] == -99990000
        assert len(gdf) == 5
