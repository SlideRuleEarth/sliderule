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
        rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "bands": ["B02"]}, "coordinates": [[-178.0, 50.7]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert len(rsps) > 0

    def test_subset1(self, init):
        time_start = "2021-01-01T00:00:00Z"
        time_end = "2021-02-01T23:59:59Z"
        polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 51.0000000001} ]
        catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
        rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "bands": ["B02"]}, "extents": [[-179.87, 50.45, -178.77, 50.75]]}
        rsps = sliderule.source("subsets", rqst)
        assert init
        assert len(rsps) > 0
        assert len(rsps['subsets'][0][0]['data']) > 0
        assert rsps['subsets'][0][0]['rows'] == 1030
        assert rsps['subsets'][0][0]['cols'] == 2504
        assert rsps['subsets'][0][0]['size'] == 5158240
        assert rsps['subsets'][0][0]['datatype'] == 1  # INT16
        assert abs(rsps['subsets'][0][0]['ulx'] - 300000) < 1e-9
        assert abs(rsps['subsets'][0][0]['uly'] - 5623518.2868507) < 1e-9
        assert rsps['subsets'][0][0]['cellsize'] == 30
        assert rsps['subsets'][0][0]['wkt'] != ""


    def test_subset167(self, init):
        time_start = "2021-01-01T00:00:00Z"
        time_end = "2021-02-01T23:59:59Z"
        polygon = [ {"lon": -177.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 51.0000000001},
                    {"lon": -179.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 49.0000000001},
                    {"lon": -177.0000000001, "lat": 51.0000000001} ]
        catalog = earthdata.stac(short_name="HLS", polygon=polygon, time_start=time_start, time_end=time_end, as_str=True)
        rqst = {"samples": {"asset": "landsat-hls", "catalog": catalog, "bands": ["VAA", "VZA", "Fmask","SAA", "SZA", "NDSI", "NDVI", "NDWI","B01", "B02", "B03", "B04", "B05", "B06","B07", "B08", "B09", "B10", "B11", "B12", "B8A"]}, "extents": [[-179.87, 50.45, -178.27, 51.44]]}
        rsps = sliderule.source("subsets", rqst)
        subsets = rsps['subsets'][0]
        assert init
        assert len(subsets) == 167

        for subset in subsets:
            assert subset['rows'] > 0
            assert subset['cols'] > 0
            assert subset['size'] > 0

            # Some rsters have datatype as INT16 others as UINT16
            assert subset['datatype'] == 1 or subset['datatype'] == 5
            assert subset['ulx'] > 0  # for this test both ulx and uly in map coords are positive
            assert subset['uly'] > 0
            assert subset['cellsize'] == 30
            assert subset['wkt'] != ""


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
