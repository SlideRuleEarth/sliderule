"""Tests for sliderule opendata worldcover raster support."""

import pytest
from pathlib import Path
import sliderule
from sliderule import raster
import geopandas as gpd

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

vrtLon = -108.1
vrtLat =   39.1

expValue     = 10
vrtFile      = '/vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt'
timeAsDateStr  = '2021-06-30 00:00:18'
timeAsUnixSecs = 1309046418  # includes 18 leap seconds

class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "esa-worldcover-10meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - expValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  timeAsUnixSecs  # datetime in seconds

    def test_time_overflow(self, init):
        gfp = gpd.GeoDataFrame(geometry=gpd.points_from_xy([-108.1, -108.3], [39.1, 39.2]), crs='EPSG:4326')
        points = [[x,y] for x,y in zip(gfp.geometry.x , gfp.geometry.y)]
        gdf = raster.sample("esa-worldcover-10meter", points)

        # Reset the index to make 'time' a regular column
        gdf_reset = gdf.reset_index()

        # Access the 'time' column
        time_values = gdf_reset['time'].tolist()
        print(time_values)

        assert [str(t) for t in time_values] == [timeAsDateStr, timeAsDateStr], \
            f"Time values do not match. Expected: {[timeAsDateStr, timeAsDateStr]}, Found: {[str(t) for t in time_values]}"

        values = gdf_reset['value']
        assert (values == expValue).all()
