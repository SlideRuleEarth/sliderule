"""Tests for h5x endpoint"""

import pytest
from sliderule import h5
import numpy


asset = "icesat2-atl09"
resource = "ATL09_20250302214125_11732601_007_01.h5"


class TestApi:
    def test_geodataframe(self, init):
        variables = [
            "met_t10m",
            "met_ts",
            "delta_time",
            "latitude",
            "longitude"
        ]
        groups = [
            "/profile_1/low_rate",
            "/profile_2/low_rate",
            "/profile_3/low_rate"
        ]
        gdf = h5.h5x(variables, resource, asset, groups, index_column="delta_time", x_column="longitude", y_column="latitude")
        assert init
        assert len(gdf) == 16995
        assert sum(gdf["srcid"]) == 16995

    def test_dataframe(self, init):
        variables = [
            "surface_sig",
        ]
        groups = [
            "/profile_1/high_rate",
            "/profile_2/high_rate",
            "/profile_3/high_rate"
        ]
        gdf = h5.h5x(variables, resource, asset, groups)
        assert init
        assert len(gdf) == 424297
        assert sum(gdf["srcid"]) == 424297
        assert abs(gdf["surface_sig"].mean() - 351.9) < 0.1
