"""Tests for sliderule arcticdem raster support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestMosaic:
    def test_vrt(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        rqst = {"samples": {"asset": "arcticdem-mosaic"}, "coordinates": [[-178.0,51.7]]}
        rsps = sliderule.source("samples", rqst)
        assert abs(rsps["samples"][0][0]["value"] - 80.713500976562) < 0.001
        assert rsps["samples"][0][0]["file"] == '/vsis3/pgc-opendata-dems/arcticdem/mosaics/v3.0/2m/70_09/70_09_2_1_2m_v3.0_reg_dem.tif'

    def test_nearestneighbour(self, domain, asset, organization, desired_nodes):
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
                  "samples": {"mosaic": {"asset": "arcticdem-mosaic"}} }
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        assert len(gdf) == 954
        assert len(gdf.keys()) == 19
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405900
        assert abs(gdf["mosaic.value"].describe()["min"] - 605.48828125) < 0.0001

    def test_zonal_stats(self, domain, asset, organization, desired_nodes):
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
                  "samples": {"mosaic": {"asset": "arcticdem-mosaic", "radius": 10.0, "zonal_stats": True}} }
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        assert len(gdf) == 954
        assert len(gdf.keys()) == 26
        assert gdf["rgt"][0] == 1160
        assert gdf["cycle"][0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405900
        assert abs(gdf["mosaic.value"].describe()["min"] - 605.48828125) < 0.0001
        assert gdf["mosaic.count"].describe()["max"] == 81
        assert gdf["mosaic.stdev"].describe()["count"] == 954
        assert gdf["mosaic.time"][0] == 1176076818.0

@pytest.mark.network
class TestStrips:
    def test_vct(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region_of_interest = [  {'lon': -46.76533411521963, 'lat': 65.4938164756588},
                                {'lon': -46.34013213284274, 'lat': 65.49860245693627},
                                {'lon': -46.35015561146599, 'lat': 65.67523503534576},
                                {'lon': -46.77853429879463, 'lat': 65.67041017142712},
                                {'lon': -46.76533411521963, 'lat': 65.4938164756588}  ]
        parms = { "poly": region_of_interest,
                  "cnf": "atl03_high",
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 40.0,
                  "res": 120.0,
                  "maxi": 5,
                  "rgt": 658,
                  "time_start":'2020-01-01',
                  "time_end":'2021-01-01',
                  "samples": {"strips": {"asset": "arcticdem-strips", "with_flags": True}} }
        gdf = icesat2.atl06p(parms, asset="icesat2", resources=['ATL03_20191108234307_06580503_005_01.h5'])
        assert len(gdf.attrs['file_directory']) == 16
        for file_id in range(16):
            assert file_id in gdf.attrs['file_directory'].keys()
            assert '/pgc-opendata-dems/arcticdem/strips/' in gdf.attrs['file_directory'][file_id]

