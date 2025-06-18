"""Tests for sliderule rema raster support."""

import pytest
from pathlib import Path
from sliderule import sliderule, raster

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -80.0
vrtLat = -80.0

vrtElevation = 328.0156250
vrtFile      = '/vsis3/sliderule/data/PGC/rema_2m_v2_0_tiles.vrt'
vrtFileTime  = 1361299922

class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "rema-mosaic"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime

    def test_vrt_with_aoi(self, init):
        bbox = [-81, -81, -79, -79]
        rqst = {"samples": {"asset": "rema-mosaic", "aoi_bbox" : bbox}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime

    def test_vrt_with_proj_pipeline(self, init):
        # Output from:     projinfo -s EPSG:4326 -t EPSG:3031 -o proj
        pipeline = "+proj=pipeline +step +proj=axisswap +order=2,1 +step +proj=unitconvert +xy_in=deg +xy_out=rad +step +proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +x_0=0 +y_0=0 +ellps=WGS84"
        rqst = {"samples": {"asset": "rema-mosaic", "proj_pipeline" : pipeline}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtElevation) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile

    def test_sample_api_serial(self, init):
        gdf = raster.sample("rema-mosaic", [[vrtLon,vrtLat]])
        assert init
        assert len(gdf) == 1
        assert abs(gdf["value"].iat[0] - vrtElevation) < sigma
        assert gdf["file"].iat[0] ==  vrtFile

    def test_sample_api_batch(self, init):
        gdf = raster.sample("rema-mosaic", [[vrtLon,vrtLat],[vrtLon+0.01,vrtLat+0.01]])
        assert init
        assert len(gdf) == 2
        assert abs(gdf["value"].iat[0] - vrtElevation) < sigma
        assert gdf["file"].iat[0] ==  vrtFile