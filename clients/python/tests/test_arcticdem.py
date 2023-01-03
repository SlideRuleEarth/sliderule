"""Tests for sliderule-python arcticdem raster support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestVrt:
    def test_vrt(self, domain, organization):
        icesat2.init(domain, organization=organization)
        rqst = {"dem-asset": "arcticdem-mosaic", "coordinates": [[-178.0,51.7]]}
        rsps = sliderule.source("samples", rqst)
        assert abs(rsps["samples"][0][0]["value"] - 80.713500976562) < 0.001
        assert rsps["samples"][0][0]["file"] == '/vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/70_09/70_09_2_1_2m_v3.0_reg_dem.tif'

    def test_sampler(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = icesat2.toregion(os.path.join(TESTDIR, "data/dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "raster": region['raster'],
                  "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "samples": ["arcticdem-mosaic"] }
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        assert len(gdf) == 964
        assert len(gdf.keys()) == 17
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405240
        assert gdf['segment_id'].describe()["max"] == 405915
        assert abs(gdf["arcticdem-mosaic-1980-01-06"].describe()["min"] - 655.14990234375) < 0.0001
