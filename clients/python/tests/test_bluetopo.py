"""Tests for sliderule bluetopo raster support."""

import pytest
from pathlib import Path
import sliderule
from sliderule import raster
import geopandas as gpd

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

lon =  -81.02
lat =   31.86

file  = '/vsis3/noaa-ocs-nationalbathymetry-pds/BlueTopo/BH4SV597/BlueTopo_'
expElevation   = -14.10
expUncertainty =   2.58
expContributor =  63846

class TestBlueTopo:
    def test_samples(self, init):
        rqst = {"samples": {"asset": "bluetopo-bathy", "bands": ["Elevation", "Uncertainty", "Contributor"]}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert file in rsps["samples"][0][0]["file"] # file names change in BlueTopo regularly
        assert rsps["samples"][0][0]["band"] == "Elevation"
        assert rsps['samples'][0][0]['value'] == pytest.approx(expElevation, rel=50)
        assert rsps["samples"][0][1]["band"] == "Uncertainty"
        assert rsps['samples'][0][1]['value'] == pytest.approx(expUncertainty, rel=50)
        assert rsps["samples"][0][2]["band"] == "Contributor"
        assert rsps['samples'][0][2]['value'] == expContributor

    def test_sample_api(self, init):
        gfp = gpd.GeoDataFrame(geometry=gpd.points_from_xy([lon, lon+0.001], [lat, lat+0.001]), crs='EPSG:4326')
        points = [[x,y] for x,y in zip(gfp.geometry.x , gfp.geometry.y)]
        gdf = raster.sample("bluetopo-bathy", points, {"bands": ["Elevation", "Uncertainty", "Contributor"]})
        assert init
        assert len(gdf) == 6

