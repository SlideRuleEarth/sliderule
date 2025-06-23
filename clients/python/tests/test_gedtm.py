"""Tests for sliderule GEDTM raster support."""

import pytest
from pathlib import Path
import sliderule
from sliderule import raster
import geopandas as gpd

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

lon = -62.0
lat = -3.0


class TestGedtm:
    def test_gedtm_30meter(self, init):
        rqst = {"samples": {"asset": "gedtm-30meter"}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)

        sample = rsps["samples"][0][0]
        file  = '/vsis3/sliderule/data/GEDTM/legendtm_rf_30m_m_s_20000101_20231231_go_epsg.4326_v20250130.tif'
        assert file in sample["file"]
        assert sample["value"] == 379
        assert sample["time"] == 1422230418

    def test_gedtm_std(self, init):
        rqst = {"samples": {"asset": "gedtm-std"}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)

        sample = rsps["samples"][0][0]
        file  = '/vsis3/sliderule/data/GEDTM/gendtm_rf_30m_std_s_20000101_20231231_go_epsg.4326_v20250209.tif'
        assert file in sample["file"]
        assert sample["value"] == 74
        assert sample["time"] == 1423094418

    def test_gedtm_dfm(self, init):
        rqst = {"samples": {"asset": "gedtm-dfm"}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)

        sample = rsps["samples"][0][0]
        file  ='/vsis3/sliderule/data/GEDTM/dfme_edtm_m_30m_s_20000101_20221231_go_epsg.4326_v20241230.tif'
        assert file in sample["file"]
        assert sample["value"] == 16
        assert sample["time"] == 1419552018

    def test_sample_api(self, init):
        gfp = gpd.GeoDataFrame(geometry=gpd.points_from_xy([lon, lon+0.001], [lat, lat+0.001]), crs='EPSG:4326')
        points = [[x,y] for x,y in zip(gfp.geometry.x , gfp.geometry.y)]
        gdf = raster.sample("gedtm-30meter", points)
        assert init
        assert len(gdf) == 2

