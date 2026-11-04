"""Tests for atl03x"""

from pathlib import Path
from sliderule import sliderule

TESTDIR = Path(__file__).parent
RESOURCES = ["ATL03LL_20261031235544_07503313_007_01.h5"]
AOI = [ { "lat": -30.00, "lon": -75.00 },
        { "lat": -31.00, "lon": -75.00 },
        { "lat": -31.00, "lon": -65.00 },
        { "lat": -30.00, "lon": -65.00 },
        { "lat": -30.00, "lon": -75.00 } ]

class TestAtl03x:
    def test_low_latency(self, init):
        parms = { "beams": ["gt1l"],
                  "cnf": 4 }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 5572
        assert len(gdf.keys()) == 17
        assert gdf.spot.value_counts()[6] == len(gdf)
        assert gdf.cycle.describe()["mean"] == 33.0
        assert gdf.atl03_cnf.value_counts()[4] == len(gdf)
