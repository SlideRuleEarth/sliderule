"""Tests for sliderule icesat2 api."""

import pytest
import sliderule
from sliderule import icesat2, gedi, earthdata
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

class TestIcesat2:
    def test_geo(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "cnf":              0,
            "srt":              icesat2.SRT_LAND,
            "atl03_geo_fields": ["solar_elevation"]
        }
        gdf = icesat2.atl06p(parms, resources=["ATL03_20181017222812_02950102_005_01.h5"])
        assert init
        assert len(gdf["solar_elevation"]) == 1180
        assert gdf['solar_elevation'].describe()["min"] - 20.803468704223633 < 0.0000001

    def test_ph(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "cnf":              0,
            "srt":              icesat2.SRT_LAND,
            "atl03_ph_fields":  ["ph_id_count"]
        }
        gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_005_01.h5")
        assert init
        assert sum(gdf["ph_id_count"]) == 626032
        assert len(gdf["ph_id_count"]) == 403462

    def test_atl06(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "cnf":              0,
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
#        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
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
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "cnf":              0,
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

class TestGedi:
    def test_l1b(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["master_frac", "rx_open"]
        }
        gdf = gedi.gedi01bp(parms, resources=['GEDI01_B_2019109210809_O01988_03_T02056_02_005_01_V002.h5'])
        assert init
        assert len(gdf) == 22
        assert gdf["beam"].unique()[0] == 0
        assert gdf.master_frac.describe()["max"] < 0.3
        assert gdf.rx_open.describe()["max"] == 2851996.0

    def test_l2a(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["energy_total", "geolocation/elev_highestreturn_a1"]
        }
        gdf = gedi.gedi02ap(parms, resources=['GEDI02_A_2019109210809_O01988_03_T02056_02_003_01_V002.h5'])
        assert init
        assert len(gdf) == 22
        assert gdf["beam"].unique()[0] == 0
        assert abs(gdf.energy_total.describe()["max"] - 7498) < 1.0
        assert abs(gdf["geolocation/elev_highestreturn_a1"].describe()["max"] - 2892) < 1.0

    def test_l4a(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["selected_algorithm", "geolocation/elev_lowestmode_a1"]
        }
        gdf = gedi.gedi04ap(parms, resources=['GEDI04_A_2019109210809_O01988_03_T02056_02_002_02_V002.h5'])
        assert init
        assert len(gdf) == 22
        assert gdf["beam"].unique()[0] == 0
        assert gdf.selected_algorithm.value_counts()[2] == 14
        assert abs(gdf["geolocation/elev_lowestmode_a1"].describe()["max"] - 2886) < 1.0

    def test_l1b_failure(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["non_existent_field", "rx_open"]
        }
        gdf = gedi.gedi01bp(parms, resources=['GEDI01_B_2019109210809_O01988_03_T02056_02_005_01_V002.h5'])
        assert init
        assert len(gdf) == 0

    def test_l2a_failure(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["non_existent_field", "geolocation/elev_highestreturn_a1"]
        }
        gdf = gedi.gedi02ap(parms, resources=['GEDI02_A_2019109210809_O01988_03_T02056_02_003_01_V002.h5'])
        assert init
        assert len(gdf) == 0

    def test_l4a_failure(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["non_existent_field", "geolocation/elev_lowestmode_a1"]
        }
        gdf = gedi.gedi04ap(parms, resources=['GEDI04_A_2019109210809_O01988_03_T02056_02_002_02_V002.h5'])
        assert init
        assert len(gdf) == 0

    def test_l2a_rh_2D(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "beam": 0,
            "anc_fields": ["rh", "geolocation/elev_highestreturn_a1"]
        }
        gdf = gedi.gedi02ap(parms, resources=['GEDI02_A_2019123154305_O02202_03_T00174_02_003_01_V002.h5'])
        assert init
        assert gdf.iloc[4].rh[3] == -2.5
