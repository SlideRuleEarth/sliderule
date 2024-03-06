"""Tests for sliderule icesat2 api."""

import pytest
import sliderule
from sliderule import icesat2
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestAncillary:
    def test_geo(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "srt":              icesat2.SRT_LAND,
            "atl03_geo_fields": ["solar_elevation"]
        }
        gdf = icesat2.atl06p(parms, resources=["ATL03_20181017222812_02950102_005_01.h5"])
        assert init
        assert len(gdf["solar_elevation"]) == 1180
        assert gdf['solar_elevation'].describe()["min"] - 20.803468704223633 < 0.0000001

    def test_ph(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "srt":              icesat2.SRT_LAND,
            "atl03_ph_fields":  ["ph_id_count"]
        }
        gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_005_01.h5")
        assert init
        assert sum(gdf["ph_id_count"]) == 626032
        assert len(gdf["ph_id_count"]) == 403462

    def test_atl06(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "srt":              icesat2.SRT_LAND,
            "atl06_fields":     ["fit_statistics/n_seg_pulses"]
        }
        gdf = icesat2.atl06s(parms, "ATL06_20181017222812_02950102_006_02.h5")
        assert init
        assert len(gdf) == 1029
        assert abs(gdf["fit_statistics/n_seg_pulses"].quantile(q=.75) - 56.234378814697266) < 0.000001

#
# This test takes too long currently.  Need to get GeoParquet support for ATL03 and ancillary data.
#
#    def test_atl08_ph(self, init):
#        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
#        parms = {
#            "poly":             region["poly"],
#            "srt":              icesat2.SRT_LAND,
#            "atl08_fields":     ["h_dif_ref"]
#        }
#        gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_006_02.h5")
#        assert init
#        assert len(gdf) == 403464
#        assert abs(gdf["h_dif_ref"].quantile(q=.75) - 10.342773) < 0.000001

    def test_atl08_phoreal(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "srt":              icesat2.SRT_LAND,
            "atl08_fields":     ["h_dif_ref", "rgt", "sigma_atlas_land%", "cloud_flag_atm%"]
        }
        gdf = icesat2.atl08(parms, "ATL03_20181017222812_02950102_006_02.h5")
        assert init
        assert len(gdf) == 819
        assert abs(gdf["h_dif_ref"].quantile(q=.50) - -0.9443359375) < 0.000001
        assert abs(gdf["rgt_y"].quantile(q=.50) - 295.0) < 0.000001
        assert abs(gdf["sigma_atlas_land%"].quantile(q=.50) - 0.24470525979995728) < 0.000001
        assert abs(gdf["cloud_flag_atm%"].quantile(q=.50) - 1.0) < 0.000001
