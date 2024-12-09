"""Tests for sliderule bluetopo raster support."""

import pytest
from pathlib import Path
import sliderule

TESTDIR = Path(__file__).parent

sigma = 1.0e-9

lon = -80.87
lat =  32.06

file  = '/vsis3/noaa-ocs-nationalbathymetry-pds/BlueTopo/BH4SX59B/BlueTopo_BH4SX59B_20241203.tiff'

expElevation   = -4.14
expUncertainty =  1.03
expContributor = 63824

@pytest.mark.network
class TestBlueTopo:
    def test_samples(self, init):
        rqst = {"samples": {"asset": "bluetopo-bathy", "bands": ["Elevation", "Uncertainty", "Contributor"]}, "coordinates": [[lon,lat]]}
        rsps = sliderule.source("samples", rqst)
        assert init
        print(rsps)
        assert rsps["samples"][0][0]["file"] ==  file
        assert rsps["samples"][0][0]["band"] == "Elevation"
        assert rsps['samples'][0][0]['value'] == pytest.approx(expElevation, rel=1e-3)
        assert rsps["samples"][0][1]["band"] == "Uncertainty"
        assert rsps['samples'][0][1]['value'] == pytest.approx(expUncertainty, rel=1e-3)
        assert rsps["samples"][0][2]["band"] == "Contributor"
        assert rsps['samples'][0][2]['value'] == expContributor
