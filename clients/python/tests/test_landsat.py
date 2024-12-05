"""Tests for sliderule landsat raster support."""

import pytest
from pathlib import Path
import os.path
from sliderule import sliderule, earthdata, icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestHLS:
    def test_samples(self, init):
        time_start = "2021-01-01T00:00:00Z"
        time_end = "2021-02-01T23:59:59Z"
        polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 51.0000000001} ]
        catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
        rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "closest_time": "2020-01-05T00:00:00Z", "bands": ["NDVI"]}, "coordinates": [[-179.0, 51.0]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert len(rsps) > 0
        assert rsps['samples'][0][0]['band'] == "NDVI"
        assert rsps['samples'][0][0]['value'] == pytest.approx(-0.259439707674, rel=1e-6)

    def test_cmr_stac(self, init):
        time_start = "2000-01-01T00:00:00Z"
        time_end = "2022-02-01T23:59:59Z"
        polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 51.0000000001} ]
        catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
        assert len(catalog) >= 6359

    def test_ndvi(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        parms = { "poly": region['poly'],
                  "srt": icesat2.SRT_LAND,
                  "cnf": icesat2.CNF_SURFACE_HIGH,
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "samples": {"ndvi": {"asset": "landsat-hls", "t0": "2021-01-01T00:00:00Z", "t1": "2021-02-01T23:59:59Z", "bands": ["NDVI"]}} }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) > 0
        assert len(gdf["ndvi.value"]) > 0
