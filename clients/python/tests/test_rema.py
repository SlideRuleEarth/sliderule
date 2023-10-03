"""Tests for sliderule rema raster support."""

import pytest
from pathlib import Path
import os.path
import sliderule
from sliderule import icesat2

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -80.0
vrtLat = -80.0

vrtElevation = 328.0156250
vrtFile      = '/vsis3/pgc-opendata-dems/rema/mosaics/v2.0/2m/2m_dem_tiles.vrt'
vrtFileTime  = 1361299922000.0

@pytest.mark.network
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
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime