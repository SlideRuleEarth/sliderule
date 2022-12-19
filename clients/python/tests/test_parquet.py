"""Tests for sliderule-python arcticdem raster support."""

import pytest
from pathlib import Path
import os
import os.path
from sliderule import icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestParquet:
    def test_parquet(self, domain, asset, organization):
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
        assert len(gdf) == 256
        assert len(gdf.keys()) == 17
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf["segment_id"][0] >= 405240
        assert gdf["segment_id"][255] <= 405609
        os.remove("testfile.parquet")

