"""Tests for sliderule icesat2 atl06-sr algorithm."""

import pytest
from sliderule import sliderule

@pytest.mark.network
class TestAtl03x:

    def test_nominal(self, init):
        resources = ["ATL03_20181019065445_03150111_005_01.h5"]
        aoi = [ { "lat": -80.75, "lon": -70.00 },
                { "lat": -81.00, "lon": -70.00 },
                { "lat": -81.00, "lon": -65.00 },
                { "lat": -80.75, "lon": -65.00 },
                { "lat": -80.75, "lon": -70.00 } ]
        parms = { "track": 1,
                  "cnf": 0,
                  "yapc": { "score": 0 },
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"] }
        gdf = sliderule.run("atl03x", parms, aoi, resources)
        assert init
        return gdf


# ########################################################
# Main
# ########################################################

if __name__ == '__main__':

    init_status = sliderule.init("localhost", organization=None, verbose=True)

    test = TestAtl03x()
    gdf = test.test_nominal(init_status)

    print(gdf)