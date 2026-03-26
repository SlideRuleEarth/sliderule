"""Tests for atl03x"""

import os
import numpy as np
from pathlib import Path
from datetime import datetime
from sliderule import sliderule

TESTDIR = Path(__file__).parent

class TestSampler:

    def test_nominal(self, init):
        parms = {
            "asset": "icesat2",
            "poly": [
                {"lat": 59.86856921063384, "lon": -44.34985645709006},
                {"lat": 59.85613150141896, "lon": -44.34985645709006},
                {"lat": 59.85613150141896, "lon": -44.30692727565953},
                {"lat": 59.86856921063384, "lon": -44.30692727565953},
                {"lat": 59.86856921063384, "lon": -44.34985645709006}
            ],
            "samples": {
                "mosaic": {
                    "asset": "arcticdem-mosaic",
                    "algorithm": "NearestNeighbour"
                }
            }
        }
        resource = "ATL03_20190105024336_01170205_006_02.h5"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert init
        assert len(gdf) == 527
        assert len(gdf.keys()) == 20
        assert "mosaic.value" in gdf

    def test_force_single(self, init):
        resource = "ATL03_20190314093716_11600203_007_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "fit": {"maxi": 3},
            "samples": {"mosaic": {"asset": "arcticdem-mosaic", "force_single_sample": "first"}}
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 197
        assert len(gdf.keys()) == 21
        assert gdf["cycle"].mean() == 2
        assert abs(gdf["mosaic.value"].mean() - 1474.9005392520041) < 0.0001, f'mosaic = {gdf["mosaic.value"].mean()}'
        assert gdf["mosaic.fileid"].mean() == 0
        assert gdf["mosaic.time_ns"].iloc[0] == datetime.strptime('2023-01-18 20:23:42', '%Y-%m-%d %H:%M:%S')

    def test_extended_columns(self, init):
        expected_columns = [
            'mosaic.value', 'mosaic.fileid', 'mosaic.time_ns',
            'mosaic.flags',
            'mosaic.stats.max', 'mosaic.stats.median', 'mosaic.stats.count', 'mosaic.stats.min', 'mosaic.stats.stdev', 'mosaic.stats.mad', 'mosaic.stats.mean',
            'mosaic.deriv.slope', 'mosaic.deriv.count', 'mosaic.deriv.aspect',
        ]
        resource = "ATL03_20190314093716_11600203_007_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "fit": {"maxi": 3},
            "samples": {"mosaic": {"asset": "arcticdem-mosaic", "force_single_sample": "first", "with_flags": True, "zonal_stats": True, "slope_aspect": True}}
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 197
        assert len(gdf.keys()) == 32
        for column in expected_columns:
            assert column in gdf

    def test_strips(self, init):
        poly = [
            {"lat": 59.86856921063384, "lon": -44.34985645709006},
            {"lat": 59.85613150141896, "lon": -44.34985645709006},
            {"lat": 59.85613150141896, "lon": -44.30692727565953},
            {"lat": 59.86856921063384, "lon": -44.30692727565953},
            {"lat": 59.86856921063384, "lon": -44.34985645709006},
        ]
        resource = "ATL03_20190105024336_01170205_006_02.h5"
        parms = {
            "asset": "icesat2",
            "poly": poly,
            "samples": {"mosaic": {"asset": "arcticdem-strips", "algorithm": "NearestNeighbour", "zonal_stats": True, "slope_aspect": True, "with_flags": True}},
        }
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        mean_values = gdf["mosaic.value"].mean()
        mean_value = mean_values[~np.isnan(mean_values)].mean()
        assert init
        assert len(gdf) == 527
        assert len(gdf.keys()) == 31
        assert gdf["cycle"].mean() == 2
        assert abs(mean_value - 307.9317036290323) < 0.0001, mean_value


    def test_force_single_sample(self, init):
        poly = [
            {"lat": 59.86856921063384, "lon": -44.34985645709006},
            {"lat": 59.85613150141896, "lon": -44.34985645709006},
            {"lat": 59.85613150141896, "lon": -44.30692727565953},
            {"lat": 59.86856921063384, "lon": -44.30692727565953},
            {"lat": 59.86856921063384, "lon": -44.34985645709006},
        ]
        resource = "ATL03_20190105024336_01170205_006_02.h5"
        assert init
        # get expected values
        parms = {
            "asset": "icesat2",
            "poly": poly,
            "samples": {"strips": {"asset": "arcticdem-strips", "algorithm": "NearestNeighbour"}},
        }
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        values = gdf["strips.value"].iloc[0]
        values = values[~np.isnan(values)]
        min_value = values.min()
        max_value = values.max()
        mean_value = values.mean()
        median_value = np.median(values)
        SIGMA = 0.0001
        # test min
        parms["samples"]["strips"]["force_single_sample"] = "min"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert abs(min_value - gdf["strips.value"].iloc[0]) < SIGMA, f"min = {min_value}, {gdf["strips.value"].iloc[0]}"
        # test max
        parms["samples"]["strips"]["force_single_sample"] = "max"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert abs(max_value - gdf["strips.value"].iloc[0]) < SIGMA, f"max = {max_value}, {gdf["strips.value"].iloc[0]}"
        # test mean
        parms["samples"]["strips"]["force_single_sample"] = "mean"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert abs(mean_value - gdf["strips.value"].iloc[0]) < SIGMA, f"mean = {mean_value}, {gdf["strips.value"].iloc[0]}"
        # test median
        parms["samples"]["strips"]["force_single_sample"] = "median"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert abs(median_value - gdf["strips.value"].iloc[0]) < SIGMA, f"median = {median_value}, {gdf["strips.value"].iloc[0]}"
        # test first (check that it exists in list)
        parms["samples"]["strips"]["force_single_sample"] = "first"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        found = False
        for value in values:
            if abs(value - gdf["strips.value"].iloc[0]) < SIGMA:
                found = True
                break
        assert found, f"first = {gdf["strips.value"].iloc[0]}, not found"
        # test last (check that it exists in list)
        parms["samples"]["strips"]["force_single_sample"] = "last"
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        found = False
        for value in values:
            if abs(value - gdf["strips.value"].iloc[0]) < SIGMA:
                found = True
                break
        assert found, f"last = {gdf["strips.value"].iloc[0]}, not found"

