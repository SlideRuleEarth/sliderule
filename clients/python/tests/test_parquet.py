"""Tests for sliderule-python parquet support."""

import pytest
from pathlib import Path
import os
import os.path
from sliderule import icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestParquet:
    def test_atl06(self, domain, asset, organization):
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
                  "output": { "path": "testfile.parquet", "format": "parquet", "open_on_complete": True } }
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        assert len(gdf) == 964
        assert len(gdf.keys()) == 19
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405240
        assert gdf['segment_id'].describe()["max"] == 405915
        os.remove("testfile.parquet")

    def test_atl03(self, domain, asset, organization):
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
                  "output": { "path": "testfile.parquet", "format": "parquet", "open_on_complete": True } }
        gdf = icesat2.atl03sp(parms, asset=asset, resources=[resource])
        assert len(gdf) == 194696
        assert len(gdf.keys()) == 17
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405240
        assert gdf['segment_id'].describe()["max"] == 405915
        os.remove("testfile.parquet")
