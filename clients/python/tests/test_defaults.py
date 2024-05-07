"""Tests for sliderule APIs."""

import pytest
import sliderule
import json



#
# Default parms
#
@pytest.mark.network
class TestDefaultParms:
    def test_parms(self, init):
        d = sliderule.source("defaults")
        assert d is not None
        assert len(d["output"]) > 0
        assert len(d["cre"]) > 0
        assert len(d["samples"]) > 0
        assert len(d["netsvc"]) > 0
        assert len(d["swot"]) > 0
        assert len(d["gedi"]) > 0
        assert len(d["icesat2"]) > 0
        assert len(d["samples"]) > 0
