"""Tests for sliderule icesat2 atl06-sr algorithm."""

import pytest
from pathlib import Path
from sliderule import sliderule

TESTDIR = Path(__file__).parent
RESOURCES = ["ATL24_20181014001920_02350103_006_02_001_01.h5"]

class TestAtl24x:
    def test_nominal(self, init):
        gdf = sliderule.run("atl24x", {}, resources=RESOURCES)
        assert init
        assert len(gdf) == 63
        assert len(gdf.keys()) == 13
        assert gdf["gt"].sum() == 2160
        assert gdf["spot"].sum() == 225
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["class_ph"].value_counts()[40] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert abs(gdf["surface_h"].mean() - -0.36007485) < 0.001, gdf["surface_h"].mean()
        assert abs(gdf["ortho_h"].mean() - -16.937626) < 0.001, gdf["ortho_h"].mean()
        assert abs(gdf["confidence"].mean() - 0.6413109983716693) < 0.001, gdf["confidence"].mean()
        assert abs(gdf["x_atc"].mean() - 7699727.727301021) < 0.001, gdf["x_atc"].mean()
        assert abs(gdf["y_atc"].mean() - -662.6006) < 0.001, gdf["y_atc"].mean()

    def test_not_compact(self, init):
        parms = {
            "atl24": {
                "compact": False,
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 63
        assert len(gdf.keys()) == 21
        assert gdf["gt"].sum() == 2160
        assert gdf["spot"].sum() == 225
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["class_ph"].value_counts()[40] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert abs(gdf["surface_h"].mean() - -0.36007485) < 0.001, gdf["surface_h"].mean()
        assert abs(gdf["ortho_h"].mean() - -16.937626) < 0.001, gdf["ortho_h"].mean()
        assert abs(gdf["confidence"].mean() - 0.6413109983716693) < 0.001, gdf["confidence"].mean()
        assert abs(gdf["x_atc"].mean() - 7699727.727301021) < 0.001, gdf["x_atc"].mean()
        assert abs(gdf["y_atc"].mean() - -662.6006) < 0.001, gdf["y_atc"].mean()
        assert abs(gdf["ellipse_h"].min() - -28.466719) < 0.0001, gdf["ellipse_h"].min()
        assert gdf["sensor_depth_exceeded"].value_counts()[0] == len(gdf)
        assert gdf["night_flag"].value_counts()[0] == 59
        assert gdf["night_flag"].value_counts()[1] == 4
        assert abs(gdf["sigma_tvu"].max() - 0.3883379) < 0.0001, gdf["sigma_tvu"].max()
        assert abs(gdf["sigma_thu"].max() - 7.0713573) < 0.0001, gdf["sigma_thu"].max()
        assert gdf["invalid_wind_speed"].value_counts()[0] == len(gdf)
        assert gdf["invalid_kd"].value_counts()[1] == len(gdf)
        assert gdf["low_confidence_flag"].value_counts()[0] == 35
        assert gdf["low_confidence_flag"].value_counts()[1] == 28

    def test_all_photons(self, init):
        parms = {
            "atl24": {
                "class_ph": ["unclassified", "bathymetry", "sea_surface"],
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 241597
        assert len(gdf.keys()) == 13
        assert gdf["gt"].sum() == 7959140
        assert gdf["spot"].sum() == 895265
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["class_ph"].value_counts()[0] == 157275
        assert gdf["class_ph"].value_counts()[40] == 63
        assert gdf["class_ph"].value_counts()[41] == 84259
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)

    def test_confidence_threshold(self, init):
        parms = {
            "atl24": {
                "confidence_threshold": 0.6,
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 35
        assert len(gdf.keys()) == 13
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert len(gdf["gt"].value_counts()) == 5
        assert len(gdf["spot"].value_counts()) == 5

    def test_flags(self, init):
        parms = {
            "atl24": {
                "invalid_kd": True,
                "invalid_wind_speed": False,
                "sensor_depth_exceeded": False,
                "night": ["on", "off"],
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 63
        assert len(gdf.keys()) == 21 # automatically makes non-compact
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert len(gdf["gt"].value_counts()) == 6
        assert len(gdf["spot"].value_counts()) == 6
        assert gdf["night_flag"].value_counts()[0] == 59
        assert gdf["night_flag"].value_counts()[1] == 4

    def test_night(self, init):
        parms = {
            "atl24": {
                "night": False,
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 59
        assert len(gdf.keys()) == 21 # automatically makes non-compact
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert gdf["night_flag"].value_counts()[0] == len(gdf)

    def test_high_confidence(self, init):
        parms = {
            "atl24": {
                "low_confidence": False,
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 35
        assert len(gdf.keys()) == 21 # automatically makes non-compact
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert gdf["low_confidence_flag"].value_counts()[0] == len(gdf)

    def test_ancillary(self, init):
        parms = {
            "atl24": {
                "anc_fields": ["index_ph", "index_seg"]
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 63
        assert len(gdf.keys()) == 15
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert gdf["index_seg"].median() == 53168
        assert gdf["index_ph"].median() == 12949300

