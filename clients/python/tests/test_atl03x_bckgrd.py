"""Tests for atl03x bckgrd ancillary fields"""

from sliderule import sliderule

RESOURCES = ["ATL03_20181019065445_03150111_006_02.h5"]
AOI = [ { "lat": -80.75, "lon": -70.00 },
        { "lat": -81.00, "lon": -70.00 },
        { "lat": -81.00, "lon": -65.00 },
        { "lat": -80.75, "lon": -65.00 },
        { "lat": -80.75, "lon": -70.00 } ]

class TestAtl03xBckgrd:
    def test_nominal(self, init):
        parms = {
            "track": 1,
            "cnf": 4,
            "srt": 3,
            "atl03_bckgrd_fields": [
                "tlm_height_band1",
                "tlm_height_band2",
                "tlm_top_band1",
                "tlm_top_band2",
            ]
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 424456
        for field in parms["atl03_bckgrd_fields"]:
            assert len(gdf[field]) == len(gdf)

    def test_surface(self, init):
        parms = {
            "track": 1,
            "cnf": 4,
            "srt": 3,
            "fit": {"maxi": 3},
            "atl03_bckgrd_fields": [
                "tlm_height_band1",
                "tlm_height_band2",
                "tlm_top_band1",
                "tlm_top_band2",
            ]
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 2880
        for field in parms["atl03_bckgrd_fields"]:
            assert len(gdf[field]) == len(gdf)

    def test_phoreal(self, init):
        parms = {
            "track": 1,
            "cnf": 4,
            "srt": 3,
            "phoreal": {},
            "atl03_bckgrd_fields": [
                "tlm_height_band1",
                "tlm_height_band2",
                "tlm_top_band1",
                "tlm_top_band2",
            ]
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 2813
        for field in parms["atl03_bckgrd_fields"]:
            assert len(gdf[field]) == len(gdf)

