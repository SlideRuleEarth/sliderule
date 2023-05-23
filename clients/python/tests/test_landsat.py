"""Tests for sliderule landsat raster support."""

import pytest
from pathlib import Path
import os.path
from sliderule import sliderule, earthdata, icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestHLS:
    def test_samples(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        time_start = "2021-01-01T00:00:00Z"
        time_end = "2021-02-01T23:59:59Z"
        polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 51.0000000001} ]
        catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
        rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "bands": ["B02"]}, "coordinates": [[-178.0, 50.7]]}
        rsps = sliderule.source("samples", rqst)

    def test_ndvi(self, domain, asset, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        time_start = "2021-01-01T00:00:00Z"
        time_end = "2021-02-01T23:59:59Z"
        catalog = earthdata.stac(short_name="HLS", polygon=region['poly'], time_start=time_start, time_end=time_end, as_str=True)
        parms = { "poly": region['poly'],
                  "raster": region['raster'],
                  "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "samples": {"ndvi": {"asset": "landsat-hls", "catalog": catalog, "bands": ["NDVI"]}} }
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
