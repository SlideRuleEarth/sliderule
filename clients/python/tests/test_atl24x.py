"""Tests for sliderule icesat2 atl24"""

import pytest
from pathlib import Path
from sliderule import sliderule

TESTDIR = Path(__file__).parent
RESOURCES = ["ATL24_20181014001920_02350103_007_01_003_01.h5"]

class TestAtl24x:
    def test_nominal(self, init):
        gdf = sliderule.run("atl24x", {}, resources=RESOURCES)
        assert init
        assert len(gdf) == 72
        assert len(gdf.keys()) == 14
        assert gdf["gt"].sum() == 2750
        assert gdf["spot"].sum() == 229
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["class_ph"].value_counts()[40] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert gdf["surface_h"].mean() == pytest.approx(-0.34916523)
        assert gdf["ortho_h"].mean() == pytest.approx(-12.123074)
        assert gdf["confidence"].mean() == pytest.approx(0.6852407)
        assert gdf["x_atc"].mean() == pytest.approx(7713100.06817151)
        assert gdf["y_atc"].mean() == pytest.approx(-1317.7193874782986)

    def test_not_compact(self, init):
        parms = {
            "atl24": {
                "compact": False,
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 72
        assert len(gdf.keys()) == 20
        assert gdf["gt"].sum() == 2750
        assert gdf["spot"].sum() == 229
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["class_ph"].value_counts()[40] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert gdf["surface_h"].mean() == pytest.approx(-0.34916523)
        assert gdf["ortho_h"].mean() == pytest.approx(-12.123074)
        assert gdf["confidence"].mean() == pytest.approx(0.6852407)
        assert gdf["x_atc"].mean() == pytest.approx(7713100.06817151)
        assert gdf["y_atc"].mean() == pytest.approx(-1317.7193874782986)
        assert gdf["ellipse_h"].min() == pytest.approx(-29.296494)
        assert gdf["night_flag"].value_counts()[0] == 55
        assert gdf["night_flag"].value_counts()[1] == 17
        assert gdf["sigma_tvu"].max() == pytest.approx(0.22828312)
        assert gdf["sigma_thu"].max() == pytest.approx(7.9573226)
        assert gdf["kd"].max() == pytest.approx(0.046218622)
        assert gdf["surface_roughness"].max() == pytest.approx(2.220303)
        assert gdf["quality_ph"].max() == 12

    def test_all_photons(self, init):
        parms = {
            "atl24": {
                "class_ph": ["unclassified", "i_dont_know", "ground", "bathymetry", "sea_surface"],
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 242777
        assert len(gdf.keys()) == 14
        assert gdf["gt"].sum() == 7982320
        assert gdf["spot"].sum() == 901207
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["class_ph"].value_counts()[0] == 154911
        assert gdf["class_ph"].value_counts()[1] == 3014
        assert gdf["class_ph"].value_counts()[2] == 4
        assert gdf["class_ph"].value_counts()[40] == 72
        assert gdf["class_ph"].value_counts()[41] == 84776
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
        assert len(gdf) == 52
        assert len(gdf.keys()) == 14
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert len(gdf["gt"].value_counts()) == 5
        assert len(gdf["spot"].value_counts()) == 5

    def test_flags(self, init):
        parms = {
            "atl24": {
                "night": ["on"],
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 17
        assert len(gdf.keys()) == 20 # automatically makes non-compact
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert len(gdf["gt"].value_counts()) == 3
        assert len(gdf["spot"].value_counts()) == 3
        assert gdf["night_flag"].value_counts()[1] == 17

    def test_ancillary(self, init):
        parms = {
            "atl24": {
                "anc_fields": ["index_ph", "index_seg"]
            }
        }
        gdf = sliderule.run("atl24x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 72
        assert len(gdf.keys()) == 16
        assert gdf["region"].value_counts()[3] == len(gdf)
        assert gdf["rgt"].value_counts()[235] == len(gdf)
        assert gdf["cycle"].value_counts()[1] == len(gdf)
        assert gdf["index_seg"].median() == 53622.0
        assert gdf["index_ph"].median() == 12997263.0

    def test_poly_subset(self, init):
        # Full run
        gdf_full = sliderule.run("atl24x", {}, resources=RESOURCES)
        assert init

        # Small polygon around a subset of the track (pick mid-quantiles)
        lat_series_full = gdf_full.geometry.y
        lon_series_full = gdf_full.geometry.x
        # Fixed bbox from quantiles to make this deterministic on the known resource
        lat_min, lat_max = lat_series_full.quantile([0.25, 0.75])
        lon_min, lon_max = lon_series_full.quantile([0.25, 0.75])
        poly = [
            {"lon": lon_min, "lat": lat_min},
            {"lon": lon_min, "lat": lat_max},
            {"lon": lon_max, "lat": lat_max},
            {"lon": lon_max, "lat": lat_min},
            {"lon": lon_min, "lat": lat_min},
        ]
        gdf_poly = sliderule.run("atl24x", {"poly": poly}, resources=RESOURCES)

        assert len(gdf_poly) <= len(gdf_full)
        lat_series_poly = gdf_poly.geometry.y
        lon_series_poly = gdf_poly.geometry.x

        assert lat_series_poly.between(lat_min, lat_max).all()
        assert lon_series_poly.between(lon_min, lon_max).all()
        assert len(gdf_poly) == 22, f"poly_len={len(gdf_poly)} expected_len=19"
        assert gdf_poly["class_ph"].value_counts().iloc[0] == len(gdf_poly)
        assert gdf_poly["region"].value_counts().iloc[0] == len(gdf_poly)
        # Per-beam counts for this resource/poly
        expected_gt_counts = {
            20: 20, # gt2r
            60: 2,  # gt3l
        }
        actual_gt_counts = gdf_poly["gt"].value_counts().to_dict()
        assert actual_gt_counts == expected_gt_counts, f"gt counts changed: {actual_gt_counts}"
        # Per-beam counts should not exceed full run
        for gt_val, full_count in gdf_full["gt"].value_counts().to_dict().items():
            poly_count = len(gdf_poly[gdf_poly["gt"] == gt_val])
            assert poly_count <= full_count, f"poly count exceeds full for gt {gt_val}: {poly_count} > {full_count}"

        # Anchor min/max coordinates for regression
        assert lat_series_poly.min() == pytest.approx(68.99866305632777)
        assert lat_series_poly.max() == pytest.approx(69.14499697426314)
        assert lon_series_poly.min() == pytest.approx(-136.85487198172936)
        assert lon_series_poly.max() == pytest.approx(-136.6947175790759)

    def test_empty_poly(self, init):
        empty_poly = [
            {"lon": 0.0, "lat": 0.0},
            {"lon": 0.1, "lat": 0.0},
            {"lon": 0.1, "lat": 0.1},
            {"lon": 0.0, "lat": 0.1},
            {"lon": 0.0, "lat": 0.0},
        ]
        gdf_empty = sliderule.run("atl24x", {"poly": empty_poly}, resources=RESOURCES)
        assert init
        assert len(gdf_empty) == 0
        # Ensure no beam slipped through
        assert gdf_empty.empty
        assert len(gdf_empty.columns) == len(sliderule.run("atl24x", {}, resources=RESOURCES).columns)
        assert gdf_empty["gt"].value_counts().empty

    def test_query(self, init):
        poly = [
            {"lat": 21.222261686673306, "lon": -73.78074797284968},
            {"lat": 21.07228912392266,  "lon": -73.78074797284968},
            {"lat": 21.07228912392266,  "lon": -73.51303956051089},
            {"lat": 21.222261686673306, "lon": -73.51303956051089},
            {"lat": 21.222261686673306, "lon": -73.78074797284968}
        ]
        gdf = sliderule.run("atl24x", {
            "poly": poly,
            "t0": "2022-02-02",
            "t1": "2023-01-01",
            "atl24": {
                "class_ph": ["unclassified", "bathymetry", "sea_surface"],
                "confidence_threshold": 0.8,
            }
        })
        assert init
        assert len(gdf) == 42749
