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
vrtFileTime  = 1309046418  # gpsTime in seconds 2021-06-30

@pytest.mark.network
class TestMosaic:
    def test_vrt(self, init):
        rqst = {"samples": {"asset": "esa-worldcover-10meter"}, "coordinates": [[vrtLon,vrtLat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        assert abs(rsps["samples"][0][0]["value"] - expValue) < sigma
        assert rsps["samples"][0][0]["file"] ==  vrtFile
        assert rsps["samples"][0][0]["time"] ==  vrtFileTime  # datetime in seconds

    def test_time_overflow(self, init):
        region = [  {"lon": -108.34, "lat": 38.89},
                    {"lon": -107.76, "lat": 38.90},
                    {"lon": -107.78, "lat": 39.26},
                    {"lon": -108.36, "lat": 39.25},
                    {"lon": -108.34, "lat": 38.89}  ]

        gfp = gpd.GeoDataFrame(geometry=gpd.points_from_xy([-108.1, -108.3], [39.1, 39.2]), crs='EPSG:4326')
        points = [[x,y] for x,y in zip(gfp.geometry.x , gfp.geometry.y)]
        gdf = raster.sample("esa-worldcover-10meter", points)

        # Reset the index to make 'time' a regular column
        gdf_reset = gdf.reset_index()

        # Access raw time column values as NumPy array
        raw_time_values = gdf_reset['time'].astype('int64')

        # Expected gps epoch time in nanoseconds
        expected_time = (vrtFileTime + 315564800) * 1e9

        print("Raw time column values (NumPy array):", raw_time_values)
        print("expected_time:", expected_time)

        assert (raw_time_values == expected_time).all()

        values = gdf_reset['value']
        assert (values == expValue).all()
