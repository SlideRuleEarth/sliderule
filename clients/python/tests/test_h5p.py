"""Tests for h5p endpoint"""

import pytest
import sliderule
from sliderule import icesat2

ATL06_FILE1 = "ATL06_20181019065445_03150111_004_01.h5"

@pytest.mark.network
class TestApi:
    def test_happy_case(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        datasets = [
            {"dataset": "/gt1l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt1r/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt2l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt2r/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt3l/land_ice_segments/h_li", "numrows": 5},
            {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = icesat2.h5p(datasets, ATL06_FILE1, asset)
        expected = {'/gt1l/land_ice_segments/h_li': [45.95665, 45.999374, 46.017857, 46.015575, 46.067562],
                    '/gt1r/land_ice_segments/h_li': [45.980865, 46.02602, 46.02262, 46.03137, 46.073578],
                    '/gt2l/land_ice_segments/h_li': [45.611526, 45.588196, 45.53242, 45.48105, 45.443752],
                    '/gt2r/land_ice_segments/h_li': [45.547, 45.515495, 45.470577, 45.468964, 45.406998],
                    '/gt3l/land_ice_segments/h_li': [45.560867, 45.611183, 45.58064, 45.579746, 45.563858],
                    '/gt3r/land_ice_segments/h_li': [45.39587, 45.43603, 45.412586, 45.40014, 45.41833]}
        for dataset in expected.keys():
            for index in range(len(expected[dataset])):
                assert round(rsps[dataset][index]) == round(expected[dataset][index])

    def test_invalid_file(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        datasets = [ {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = icesat2.h5p(datasets, "invalid_file.h5", asset)
        assert len(rsps) == 0

    def test_invalid_asset(self, domain, organization):
        icesat2.init(domain, organization=organization)
        datasets = [ {"dataset": "/gt3r/land_ice_segments/h_li", "numrows": 5} ]
        rsps = icesat2.h5p(datasets, ATL06_FILE1, "invalid-asset")
        assert len(rsps) == 0

    def test_invalid_dataset(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        datasets = [ {"dataset": "/gt3r/invalid", "numrows": 5} ]
        rsps = icesat2.h5p(datasets, ATL06_FILE1, asset)
        assert len(rsps) == 0