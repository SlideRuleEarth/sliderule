"""Tests for sliderule arcticdem raster support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -150.0
vrtLat =  70.0
vrtElevation = 116.250000000
vrtFile      = '/vsis3/pgc-opendata-dems/arcticdem/mosaics/v4.1/2m_dem_tiles.vrt'
vrtFileTime  = 1358108640000.0

@pytest.mark.network
class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "arcticdem-mosaic"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime


    def test_vrt_with_aoi(self, init):
        bbox = [-179, 50, -177, 52]
        rqst = {"samples": {"asset": "arcticdem-mosaic", "aoi_bbox" : bbox}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime

    def test_vrt_with_proj_pipeline(self, init):
        # Output from:     projinfo -s EPSG:4326 -t EPSG:3413 -o proj
        pipeline = "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=90 +lat_ts=70 +lon_0=-45 +x_0=0 +y_0=0 +ellps=WGS84"
        rqst = {"samples": {"asset": "arcticdem-mosaic", "proj_pipeline" : pipeline}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime


    def test_nearestneighbour(self, init):
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
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 957
        assert len(gdf.keys()) == 20
        assert gdf["rgt"].iloc[0] == 1160
        assert gdf["cycle"].iloc[0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405902
        assert abs(gdf["mosaic.value"].describe()["min"] - 600.4140625) < sigma

    def test_zonal_stats(self, init):
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
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 957
        assert len(gdf.keys()) == 27
        assert gdf["rgt"].iloc[0] == 1160
        assert gdf["cycle"].iloc[0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405902
        assert abs(gdf["mosaic.value"].describe()["min"] - 600.4140625) < sigma
        assert gdf["mosaic.count"].describe()["max"] == 81
        assert gdf["mosaic.stdev"].describe()["count"] == 957
        assert gdf["mosaic.time"].iloc[0] == vrtFileTime

@pytest.mark.network
class TestStrips:
    def test_indexed_raster(self, init):
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
        gdf = icesat2.atl06p(parms, resources=['ATL03_20191108234307_06580503_005_01.h5'])
        assert init
        assert len(gdf.attrs['file_directory']) == 32        
        for file_id in range(16):
            assert file_id in gdf.attrs['file_directory'].keys()
            assert '/pgc-opendata-dems/arcticdem/strips/' in gdf.attrs['file_directory'][file_id]

