"""Tests for sliderule icesat2 atl06-sr algorithm."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule
from sliderule import icesat2
from pathlib import Path
import os.path

@pytest.mark.network
class TestAlgorithm:
    def test_atl06(self, server, asset):
        icesat2.init(server)
        resource = "ATL03_20181019065445_03150111_004_01.h5"
        parms = { "cnf": "atl03_high",
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl06(parms, resource, asset)
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["h_mean"]) == 622423

    def test_atl03(self, server, asset):
        icesat2.init(server)
        resource = "ATL03_20181019065445_03150111_004_01.h5"
        region = [ { "lat": -80.75, "lon": -70.00 },
                   { "lat": -81.00, "lon": -70.00 },
                   { "lat": -81.00, "lon": -65.00 },
                   { "lat": -80.75, "lon": -65.00 },
                   { "lat": -80.75, "lon": -70.00 } ]
        parms = { "poly": region,
                  "track": 1,
                  "cnf": 0,
                  "pass_invalid": True,
                  "yapc": { "score": 0 },
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"],
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 20.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl03s(parms, resource, asset)
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["height"]) == 488673
