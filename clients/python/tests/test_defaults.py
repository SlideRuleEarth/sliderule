"""Tests for sliderule APIs."""

import pytest
import sliderule

#
# Default parms
#
@pytest.mark.network
class TestDefaultParms:
    def test_parms(self, init):
        d = sliderule.source("defaults")
        assert init
        assert d["output"]["as_geo"]
        assert d["cre"]["timeout"] == 600
        assert d["samples"]["sampling_algo"] == "NearestNeighbour"
        assert d["rqst"]["cluster_size_hint"] == 0
        assert d["icesat2"]["surface_type"] == "SRT_LAND_ICE"
        assert d["gedi"]["projection"] == "AUTOMATIC"
