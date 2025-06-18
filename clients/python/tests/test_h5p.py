"""Tests for h5p endpoint"""

import pytest
from sliderule import h5

ATL06_FILE1 = "ATL06_20181019065445_03150111_005_01.h5"

class TestApi:
    def test_happy_case(self, init):
        datasets = [
            {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = h5.h5p(datasets, ATL06_FILE1, "icesat2")
        assert init
        expected = {'/gt1l/land_ice_segments/h_li': [45.95665, 45.999374, 46.017857, 46.015575, 46.067562],
                    '/gt1r/land_ice_segments/h_li': [45.980865, 46.02602, 46.02262, 46.03137, 46.073578],
                    '/gt2l/land_ice_segments/h_li': [45.611526, 45.588196, 45.53242, 45.48105, 45.443752],
                    '/gt2r/land_ice_segments/h_li': [45.547, 45.515495, 45.470577, 45.468964, 45.406998],
                    '/gt3l/land_ice_segments/h_li': [45.560867, 45.611183, 45.58064, 45.579746, 45.563858],
                    '/gt3r/land_ice_segments/h_li': [45.39587, 45.43603, 45.412586, 45.40014, 45.41833]}
        for dataset in expected.keys():
            for index in range(len(expected[dataset])):
                assert round(rsps[dataset][index]) == round(expected[dataset][index])

    def test_invalid_file(self, init):
        datasets = [ {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = h5.h5p(datasets, "invalid_file.h5", "icesat2")
        assert init
        assert len(rsps) == 0

    def test_invalid_asset(self, init):
        datasets = [ {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = h5.h5p(datasets, ATL06_FILE1, "invalid-asset")
        assert init
        assert len(rsps) == 0

    def test_invalid_dataset(self, init):
        datasets = [ {"dataset": "/gt3r/invalid", "numrows": 5} ]
        rsps = h5.h5p(datasets, ATL06_FILE1, "icesat2")
        assert init
        assert len(rsps) == 0


    def test_missing_invalid(self, init):
        resource = "ATL03_20181014012500_02350113_006_02.h5"
        r1 = h5.h5p([
            {'dataset': "/gt1r/signal_find_output/ocean/delta_time"},
            {'dataset': "/gt1l/signal_find_output/ocean/delta_time"}
        ], resource, "icesat2")
        assert "/gt1r/signal_find_output/ocean/delta_time" in r1
        assert "/gt1l/signal_find_output/ocean/delta_time" not in r1

    def test_missing_valid(self, init):
        resource = "ATL03_20181014012500_02350113_006_02.h5"
        r2 = h5.h5p([
            {'dataset': "/gt2r/signal_find_output/ocean/delta_time"},
            {'dataset': "/gt2l/signal_find_output/ocean/delta_time"}
        ], resource, "icesat2")
        assert "/gt2r/signal_find_output/ocean/delta_time" in r2
        assert "/gt2l/signal_find_output/ocean/delta_time" not in r2

    def test_missing_valid_numrows_too_high(self, init):
        resource = "ATL03_20181014012500_02350113_006_02.h5"
        r3 = h5.h5p([
            {'dataset': "/gt2r/signal_find_output/ocean/delta_time", 'numrows': 10},
            {'dataset': "/gt2l/signal_find_output/ocean/delta_time", 'numrows': 10}
        ], resource, "icesat2")
        assert "/gt2r/signal_find_output/ocean/delta_time" in r3
        assert "/gt2l/signal_find_output/ocean/delta_time" not in r3

    def test_missing_valid_numrows_okay(self, init):
        resource = "ATL03_20181014012500_02350113_006_02.h5"
        r4 = h5.h5p([
            {'dataset': "/gt2r/signal_find_output/ocean/delta_time", 'numrows': 3},
            {'dataset': "/gt2l/signal_find_output/ocean/delta_time", 'numrows': 3}
        ], resource, "icesat2")
        assert "/gt2r/signal_find_output/ocean/delta_time" in r4
        assert "/gt2l/signal_find_output/ocean/delta_time" not in r4

    def test_numrows_zero(self, init):
        resource = "ATL03_20181014012500_02350113_006_02.h5"
        r5 = h5.h5p([
            {'dataset': "/gt2r/signal_find_output/ocean/delta_time", 'numrows': 0},
            {'dataset': "/gt2l/signal_find_output/ocean/delta_time", 'numrows': 0}
        ], resource, "icesat2")
        assert len(r5) == 0

    def test_numrows_too_high(self, init):
        resource = "ATL03_20181014012500_02350113_006_02.h5"
        r6 = h5.h5("/gt1r/signal_find_output/ocean/delta_time", resource, "icesat2", numrows=100000)
        assert len(r6) == 0

