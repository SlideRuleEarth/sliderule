"""Tests for sliderule icesat2 atl06-sr algorithm."""

import pytest
from sliderule import icesat2

class Test3dCRS:
    def test_atl06p(self, init):
        resource = "ATL03_20181019065445_03150111_005_01.h5"
        parms = { "cnf": "atl03_high",
                  "srt": 3,
                  "ats": 20.0,
                  "cnt": 10,
                  "len": 40.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl06p(parms, resources=[resource], height_key='h_mean')
        assert init
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["h_mean"]) == 622419

    def test_atl03sp(self, init):
        resource = "ATL03_20181019065445_03150111_005_01.h5"
        region = [ { "lat": -80.75, "lon": -70.00 },
                   { "lat": -81.00, "lon": -70.00 },
                   { "lat": -81.00, "lon": -65.00 },
                   { "lat": -80.75, "lon": -65.00 },
                   { "lat": -80.75, "lon": -70.00 } ]
        parms = { "poly": region,
                  "track": 1,
                  "cnf": 0,
                  "srt": 3,
                  "pass_invalid": True,
                  "yapc": { "score": 0 },
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"],
                  "ats": 10.0,
                  "cnt": 5,
                  "len": 20.0,
                  "res": 20.0,
                  "maxi": 1 }
        gdf = icesat2.atl03sp(parms, resources=[resource], height_key='height')
        assert init
        assert min(gdf["rgt"]) == 315
        assert min(gdf["cycle"]) == 1
        assert len(gdf["height"]) == 488690

    def test_atl08p(self, init):
        resource = "ATL03_20181213075606_11560106_005_01.h5"
        region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                   {"lon": -107.7677425431139, "lat": 38.90611184543033},
                   {"lon": -107.7818591266989, "lat": 39.26613714985466},
                   {"lon": -108.3605610678553, "lat": 39.25086131372244},
                   {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        parms = {
            "poly": region,
            "srt": icesat2.SRT_LAND,
            "len": 100,
            "res": 100,
            "pass_invalid": True,
            "atl08_class": ["atl08_ground", "atl08_canopy", "atl08_top_of_canopy"],
            "phoreal": {"binsize": 1.0, "geoloc": "center", "use_abs_h": False, "send_waveform": True}
        }
        gdf = icesat2.atl08p(parms, resources=[resource], height_key='h_te_median')
        assert init
        assert len(gdf) > 0

