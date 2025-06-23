import pytest
from sliderule import icesat2
import time

resource = "ATL03_20181019065445_03150111_005_01.h5"
region = [ { "lat": -80.75, "lon": -70.00 },
           { "lat": -81.00, "lon": -70.00 },
           { "lat": -81.00, "lon": -65.00 },
           { "lat": -80.75, "lon": -65.00 },
           { "lat": -80.75, "lon": -70.00 } ]

parms = { "poly": region,
          "track": 1,
          "srt": 3 }

def assert_gdf(gdf):
    assert min(gdf["pair"]) == 0
    assert min(gdf["region"]) == 11
    assert min(gdf["cycle"]) == 1
    assert min(gdf["track"]) == 1
    assert min(gdf["rgt"]) == 315
    assert min(gdf["segment_id"]) == 1453625
    assert max(gdf["segment_id"]) == 1455074
    assert min(gdf["segment_ph_cnt"]) == 53
    assert max(gdf["segment_ph_cnt"]) == 312
    assert min(gdf["spot"]) == 5
    assert max(gdf["spot"]) == 6


class TestAlgorithm:
    def test_atl03v(self, init, performance):
        perf_start = time.perf_counter()
        gdf = icesat2.atl03v(parms, resource)
        assert init
        assert_gdf(gdf)
        assert not performance or (time.perf_counter() - perf_start) < 40

    def test_atl03vp(self, init, performance):
        perf_start = time.perf_counter()
        gdf = icesat2.atl03vp(parms, resources=[resource])
        assert init
        assert_gdf(gdf)
        assert not performance or (time.perf_counter() - perf_start) < 40
