"""Tests for sliderule subsetting code."""

import pytest
from sliderule import icesat2, sliderule
from pathlib import Path
import os.path

import pandas as pd
import numpy as np

TESTDIR = Path(__file__).parent

class TestSubsetting:
    def test_rasterize(self, init):
        resource = "ATL03_20181017222812_02950102_007_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region['poly'],
            "region_mask": region['raster'],
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 956
        assert abs(gdf["h_mean"].describe()["mean"] - 1750.5657658961802) < 0.1, f'h_mean = {gdf["h_mean"].describe()["mean"]}'
        assert abs(gdf["y_atc"].describe()["mean"] - -5517.46630859375) < 0.1, f'y_atc = {gdf["y_atc"].describe()["mean"]}'

        # This part is a regression test for issue #358
        # Compare values for h_mean and y_atc and validate segment_ids against expected values

        # Load expected data
        expected_df = pd.read_csv(os.path.join(TESTDIR, "data", "expected_subsetting_values.txt"))

        # Ensure same length
        assert len(expected_df) == len(gdf), "Mismatch in row count"

        # Compare each value with high precision
        for i in range(len(expected_df)):
            assert expected_df["segment_id"].iloc[i] == gdf["segment_id"].iloc[i], f"Row {i} mismatch in segment_id"
            assert np.isclose(expected_df["h_mean"].iloc[i], gdf["h_mean"].iloc[i], atol=1e-14), f"Row {i} mismatch in h_mean"
            assert np.isclose(expected_df["y_atc"].iloc[i], gdf["y_atc"].iloc[i], atol=1e-14), f"Row {i} mismatch in y_atc"

    def test_180_edge(self, init):
        resource = "ATL03_20221012073759_03291712_007_01.h5"
        poly = [ { "lat": -73.0, "lon": 155.00 },
                 { "lat": -74.0, "lon": 155.00 },
                 { "lat": -74.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": 155.00 } ]
        parms = {
            "poly": poly,
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 33998
        assert abs(gdf["h_mean"].describe()["mean"] - 1963.296349059903) < 0.01, f'h_mean = {gdf["h_mean"].describe()["mean"]}'
        assert abs(gdf["y_atc"].describe()["mean"] - 1.9260876178741455) < 0.01, f'y_atc = {gdf["y_atc"].describe()["mean"]}'

    def test_180_edge_plate_carree(self, init):
        resource = "ATL03_20221012073759_03291712_007_01.h5"
        poly = [ { "lat": -73.0, "lon": -180.00 },
                 { "lat": -74.0, "lon": -180.00 },
                 { "lat": -74.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": 180.00 },
                 { "lat": -73.0, "lon": -180.00 } ]
        parms = {
            "poly": poly,
            "proj": "plate_carree",
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 33972
        assert abs(gdf["h_mean"].describe()["mean"] - 2105.146177305181) < 0.01, f'h_mean = {gdf["h_mean"].describe()["mean"]}'
        assert abs(gdf["y_atc"].describe()["mean"] - 0.1547791212797165) < 0.01, f'y_atc = {gdf["y_atc"].describe()["mean"]}'

    def test_150_translation(self, init):
        resource = "ATL03_20221009072040_02831712_007_01.h5"
        poly = [ { "lat": -73.0, "lon": 160.00 },
                 { "lat": -74.0, "lon": 160.00 },
                 { "lat": -74.0, "lon": 170.00 },
                 { "lat": -73.0, "lon": 170.00 },
                 { "lat": -73.0, "lon": 160.00 } ]
        parms = {
            "poly": poly,
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_LOW,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0 }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 25860
        assert abs(gdf["h_mean"].describe()["mean"] - 100.34022015114549) < 0.01, f'h_mean = {gdf["h_mean"].describe()["mean"]}'
        assert abs(gdf["y_atc"].describe()["mean"] - 80.0853271484375) < 0.01, f'y_atc = {gdf["y_atc"].describe()["mean"]}'
