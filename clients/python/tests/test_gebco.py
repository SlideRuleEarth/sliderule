"""Tests for sliderule gebco raster support."""

import pytest
from pathlib import Path
import sliderule
from sliderule import raster

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

lon = -20
lat = -20

expected_tid = 40  # fmask for type identifier from TID grid

expected_depth_2023 = -4933  # meters
expected_file_2023  = '/vsis3/sliderule/data/GEBCO/2023/cog_gebco_2023_sub_ice_n0.0_s-90.0_w-90.0_e0.0.tif'

expected_depth_2024 = -4940  # different from 2023
expected_file_2024  = '/vsis3/sliderule/data/GEBCO/2024/cog_gebco_2024_sub_ice_n0.0_s-90.0_w-90.0_e0.0.tif'

class TestGebco:
    def test_samples_2023(self, init):
        rqst = {"samples": {"asset": "gebco-bathy", "with_flags": True, "bands": ["2023"]}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert rsps["samples"][0][0]["value"] == expected_depth_2023
        assert rsps["samples"][0][0]["file"] ==  expected_file_2023
        assert rsps["samples"][0][0]["flags"] == expected_tid

    def test_samples_2024(self, init):
        rqst = {"samples": {"asset": "gebco-bathy", "with_flags": True, "bands": ["2024"]}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert rsps["samples"][0][0]["value"] == expected_depth_2024
        assert rsps["samples"][0][0]["file"] ==  expected_file_2024
        assert rsps["samples"][0][0]["flags"] == expected_tid

    # Test default data set (no bands parameter)
    def test_samples_default(self, init):
        rqst = {"samples": {"asset": "gebco-bathy", "with_flags": True}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert rsps["samples"][0][0]["value"] == expected_depth_2024
        assert rsps["samples"][0][0]["file"]  == expected_file_2024
        assert rsps["samples"][0][0]["flags"] == expected_tid

    def test_sample_api(self, init):
        gdf = raster.sample("gebco-bathy", [[lon,lat],[lon+0.01,lat+0.01]])
        assert init
        assert len(gdf) == 2
        assert gdf["value"].iloc[0] == expected_depth_2024
        assert gdf["file"].iloc[0]  == expected_file_2024
