"""Tests for sliderule-python landsat raster support."""

import pytest
from pathlib import Path
import os.path
from sliderule import sliderule, earthdata

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestHLS:
    def test_samples(self, domain, organization):
        sliderule.init(domain, organization=organization)
        time_start = "2021-01-01T00:00:00Z"
        time_end = "2022-01-01T23:59:59Z"
        polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 51.0000000001} ]
        catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
        print(catalog)
        rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog}, "coordinates": [[-178.0, 51.7]]}
        rsps = sliderule.source("samples", rqst)
