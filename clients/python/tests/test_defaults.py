"""Tests for sliderule APIs."""

import pytest
from sliderule import sliderule

#
# Default parms
#
class TestDefaultParms:
    def test_parms(self, init):
        d = sliderule.source("defaults")
        assert init
        assert d["icesat2"]["output"]["as_geo"] == False, f'icesat2 output has invalid as_geo field: {d["icesat2"]["output"]}'
        assert d["cre"]["timeout"] == 600, f'container runtime environment has invalid timeout: {d["cre"]}'
        assert len(d["core"]["samples"]) == 0, f'core has invalid samples parameters: {d["core"]["samples"]}'
        assert d["core"]["cluster_size_hint"] == 0, f'core has invalid cluster size hint: {d["core"]["cluster_size_hint"]}'
        assert d["icesat2"]["srt"] == "dynamic", f'icesat2 has invalid surface reference type: {d["icesat2"]["srt"]}'
        assert d["gedi"]["proj"] == "auto", f'gedi has invalid projection: {d["gedi"]["proj"]}'
