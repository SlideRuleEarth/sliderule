"""Tests for sliderule arcticdem raster support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2
from sliderule import raster
from sliderule import earthdata
from sliderule import earthdata
from sliderule.earthdata import __cmr_collection_query as cmr_collection_query
from sliderule.earthdata import __cmr_max_version as cmr_max_version

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -150.0
vrtLat =  70.0
vrtElevation = 116.25
vrtFile      = '/vsis3/sliderule/data/PGC/arcticdem_2m_v4_1_tiles.vrt'
vrtFileTime  = 1358108640.0

class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "arcticdem-mosaic"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime

    def test_sample_api_serial_zonal_and_slope_aspect(self, init):
        gdf = raster.sample("arcticdem-mosaic", [[vrtLon,vrtLat]], parms={"zonal_stats": True, "radius": 100, "slope_aspect": True, "slope_scale_length": 40})
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtElevation) < sigma
        assert gdf["file"].iat[0] ==  vrtFile
        assert gdf["count"].iat[0] == 7845         # zonal stats count - valid pixels used for calculation of zonal stats
        assert gdf["slope_count"].iat[0] == 441    # slope aspect count - valid pixels used for calculation of spatial derivatives
        assert abs(gdf["slope"].iat[0] - 0.21147272317954) < sigma
        assert abs(gdf["aspect"].iat[0] - 288.63621380242) < sigma

    def test_sample_api_batch_zonal_and_slope_aspect(self, init):
        gdf = raster.sample("arcticdem-mosaic", [[vrtLon,vrtLat],[vrtLon+0.01,vrtLat+0.01]], parms={"zonal_stats": True, "radius": 100, "slope_aspect": True, "slope_scale_length": 40})
        assert init
        assert len(gdf) == 2
        # First point
        assert abs(gdf["value"].iat[0] - vrtElevation) < sigma
        assert gdf["file"].iat[0] ==  vrtFile
        assert gdf["count"].iat[0] == 7845         # zonal stats count - valid pixels used for calculation of zonal stats
        assert gdf["slope_count"].iat[0] == 441    # slope aspect count - valid pixels used for calculation of spatial derivatives
        assert abs(gdf["slope"].iat[0] - 0.21147272317954) < sigma
        assert abs(gdf["aspect"].iat[0] - 288.63621380242) < sigma
        # Second point
        assert abs(gdf["value"].iat[1] - 111.515625) < sigma
        assert gdf["file"].iat[1] ==  vrtFile
        assert gdf["count"].iat[1] == 7845         # zonal stats count - valid pixels used for calculation of zonal stats
        assert gdf["slope_count"].iat[1] == 441    # slope aspect count - valid pixels used for calculation of spatial derivatives
        assert abs(gdf["slope"].iat[1] - 0.43360241027112) < sigma
        assert abs(gdf["aspect"].iat[1] - 248.46500670648) < sigma

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
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "srt": 3,
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
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "srt": 3,
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "samples": {"mosaic": {"asset": "arcticdem-mosaic", "radius": 10, "zonal_stats": True}} }
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

    def test_slope_aspect(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "srt": 3,
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "samples": {"mosaic": {"asset": "arcticdem-mosaic", "slope_aspect": True, "slope_scale_length": 40}} }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 957
        assert len(gdf.keys()) == 23
        assert gdf["rgt"].iloc[0] == 1160
        assert gdf["cycle"].iloc[0] == 2
        assert gdf['segment_id'].describe()["min"] == 405231
        assert gdf['segment_id'].describe()["max"] == 405902
        assert abs(gdf["mosaic.value"].describe()["min"] - 600.4140625) < sigma
        assert gdf["mosaic.slope"].describe()["count"] == 957
        assert gdf["mosaic.aspect"].describe()["count"] == 957
        assert gdf["mosaic.time"].iloc[0] == vrtFileTime

        assert gdf["mosaic.count"].iloc[0] == 441
        assert abs(gdf["mosaic.slope"].iloc[0]  - 17.764184149536096) < sigma
        assert abs(gdf["mosaic.aspect"].iloc[0] - 266.8953482375976) < sigma
        # print(gdf[["mosaic.slope", "mosaic.aspect", "mosaic.count"]])

    def test_slope_aspect_and_zonal_stats(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = { "poly": region['poly'],
                  "cnf": "atl03_high",
                  "srt": 3,
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1,
                  "samples": {"mosaic": {"asset": "arcticdem-mosaic", "zonal_stats": True, "radius": 100, "slope_aspect": True}} }
        gdf = icesat2.atl06p(parms, resources=[resource])
        assert init
        assert len(gdf) == 957
        assert len(gdf.keys()) == 27
        # The legacy streaming sampling code does not support zonal stats and slope/aspect together, if both are requested only zonal stats is returned
        assert 'mosaic.stdev' in gdf.columns
        assert 'mosaic.slope' not in gdf.columns


@pytest.mark.external
class TestStrips:
    def test_indexed_raster(self, init):
        region_of_interest = [  {'lon': -46.76533411521963, 'lat': 65.4938164756588},
                                {'lon': -46.34013213284274, 'lat': 65.49860245693627},
                                {'lon': -46.35015561146599, 'lat': 65.67523503534576},
                                {'lon': -46.77853429879463, 'lat': 65.67041017142712},
                                {'lon': -46.76533411521963, 'lat': 65.4938164756588}  ]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=region_of_interest, as_str=True)
        parms = { "poly": region_of_interest,
                  "cnf": "atl03_high",
                  "srt": 3,
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 40.0,
                  "res": 120.0,
                  "maxi": 5,
                  "rgt": 658,
                  "time_start":'2020-01-01',
                  "time_end":'2021-01-01',
                  "samples": {"strips": {"asset": "arcticdem-strips", "with_flags": True, "catalog": catalog}} }
        gdf = icesat2.atl06p(parms, resources=['ATL03_20191108234307_06580503_005_01.h5'])
        assert init
        assert len(gdf.attrs['file_directory']) == 14
        for file_id in range(0, 16, 2):
            assert file_id in gdf.attrs['file_directory'].keys()
            assert '/pgc-opendata-dems/arcticdem/strips/' in gdf.attrs['file_directory'][file_id]
            assert '_dem.tif' in gdf.attrs['file_directory'][file_id]  # only dems, no flags

    def test_sample_api_serial(self, init):
        arcticdem_test_point = [
            [
                { "lon": -150.0, "lat": 70.0 }
            ]
        ]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=arcticdem_test_point, as_str=True)
        gdf = raster.sample("arcticdem-strips", [[vrtLon,vrtLat]], parms={"catalog": catalog})
        assert init
        assert len(gdf) == 11

    def test_sample_api_batch(self, init):
        arcticdem_test_region = [
            [
                { "lon": -150.0, "lat": 70.0 },
                { "lon": -150.0, "lat": 71.0 },
                { "lon": -149.0, "lat": 71.0 },
                { "lon": -149.0, "lat": 70.0 },
                { "lon": -150.0, "lat": 70.0 }
            ]
        ]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=arcticdem_test_region, as_str=True)
        gdf = raster.sample("arcticdem-strips", [[vrtLon,vrtLat],[vrtLon+0.01,vrtLat+0.01]], parms={"catalog": catalog})
        assert init
        assert len(gdf) == 22

