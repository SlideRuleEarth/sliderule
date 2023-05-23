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
    def test_atl06(self, domain, asset, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
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
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        os.remove("testfile1.parquet")
        assert len(gdf) == 954
        assert len(gdf.keys()) == 16
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405900

    def test_atl03(self, domain, asset, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
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
        gdf = icesat2.atl03sp(parms, asset=asset, resources=[resource])
        os.remove("testfile2.parquet")
        assert len(gdf) == 194544
        assert len(gdf.keys()) == 17
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405900

    def test_atl06_index(self, domain, asset, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
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
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        os.remove("testfile3.parquet")
        assert len(gdf) == 265
        assert gdf.index.values.min() == numpy.datetime64('2018-10-17T22:31:17.350047232')
        assert gdf.index.values.max() == numpy.datetime64('2018-10-17T22:31:19.582526976')

    def test_atl03_index(self, domain, asset, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
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
        gdf = icesat2.atl03sp(parms, asset=asset, resources=[resource])
        os.remove("testfile4.parquet")
        assert len(gdf) == 20660
        assert gdf.index.values.min() == numpy.datetime64('2018-10-17T22:31:17.349347328')
        assert gdf.index.values.max() == numpy.datetime64('2018-10-17T22:31:19.582347520')
