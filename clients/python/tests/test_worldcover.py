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

vrtValue     = 10
vrtFile      = '/vsis3/sliderule/data/WORLDCOVER/ESA_WorldCover_10m_2021_v200_Map.vrt'
vrtFileTime  = 1309046418000

@pytest.mark.network
class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "esa-worldcover-10meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - vrtValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime

    def test_time_overflow(self, init):
        region = [  {"lon": -108.34, "lat": 38.89},
                    {"lon": -107.76, "lat": 38.90},
                    {"lon": -107.78, "lat": 39.26},
                    {"lon": -108.36, "lat": 39.25},
                    {"lon": -108.34, "lat": 38.89}  ]

        gfp = gpd.GeoDataFrame(geometry=gpd.points_from_xy([-108.1, -108.3], [39.1, 39.2]), crs='EPSG:4326')
        points = [[x,y] for x,y in zip(gfp.geometry.x , gfp.geometry.y)]
        gdf = raster.sample("esa-worldcover-10meter", points)

        expected_time  = "2021-06-30 00:00:18"
        expected_value = 10.0

        # Check if all timestamps match the expected_time (used as index)
        assert all(str(index) == expected_time for index in gdf.index), \
            f"Time mismatch. Expected: {expected_time}, Found: {gdf.index.tolist()}"

        # Check if all values match the expected value
        assert all(gdf["value"] == expected_value), \
            f"Value mismatch. Expected: {expected_value}, Found: {gdf['value'].tolist()}"

