"""Tests for sliderule parquet support."""

import pytest
from pathlib import Path
import numpy
import os
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestParquet:
    def test_atl06(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "output": { "path": "testfile1.parquet", "format": "parquet", "open_on_complete": True } }
        gdf = icesat2.atl06p(parms, resources=[resource])
        os.remove("testfile1.parquet")
        assert init
        assert len(gdf) == 957
        assert len(gdf.keys()) == 16
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405902

    def test_atl06_non_geo(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "output": { "path": "testfile5.parquet", "format": "parquet", "open_on_complete": True, "as_geo": False } }
        gdf = icesat2.atl06p(parms, resources=[resource])
        os.remove("testfile5.parquet")
        assert init
        assert len(gdf) == 957
        assert len(gdf.keys()) == 17
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405902

    def test_atl03(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "output": { "path": "testfile2.parquet", "format": "parquet", "open_on_complete": True } }
        gdf = icesat2.atl03sp(parms, resources=[resource])
        os.remove("testfile2.parquet")
        assert init
        assert len(gdf) == 190491
        assert len(gdf.keys()) == 23
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405902

    def test_atl06_index(self, init):
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_HIGH,
            "ats": 10.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "output": { "path": "testfile3.parquet", "format": "parquet", "open_on_complete": True } }
        gdf = icesat2.atl06p(parms, resources=[resource])
        os.remove("testfile3.parquet")
        assert init
        assert len(gdf) == 265
        assert gdf.index.values.min() == numpy.datetime64('2018-10-17T22:31:17.350047744')
        assert gdf.index.values.max() == numpy.datetime64('2018-10-17T22:31:19.582527744')

    def test_atl03_index(self, init):
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_HIGH,
            "ats": 10.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "output": { "path": "testfile4.parquet", "format": "parquet", "open_on_complete": True } }
        gdf = icesat2.atl03sp(parms, resources=[resource])
        os.remove("testfile4.parquet")
        assert init
        assert len(gdf) == 20642
        assert gdf.index.values.min() == numpy.datetime64('2018-10-17T22:31:17.349347328')
        assert gdf.index.values.max() == numpy.datetime64('2018-10-17T22:31:19.582347520')
