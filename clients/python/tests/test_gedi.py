"""Tests for sliderule gedi plugin."""

import pytest
from sliderule import sliderule, gedi, earthdata, icesat2
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestL1B:
    def test_gedi(self, domain, organization, desired_nodes):
        gedi.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        resource = "GEDI01_B_2019109210809_O01988_03_T02056_02_005_01_V002.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "degrade_flag": 0,
            "quality_flag": 1,
            "beam": 0
        }
        gdf = gedi.gedi01bp(parms, resources=[resource])
        assert gdf.describe()["beam"]["mean"] == 0.0
        assert gdf.describe()["flags"]["mean"] == 0.0
        assert gdf.describe()["tx_size"]["mean"] == 128.0
        assert gdf.describe()["rx_size"]["min"] == 737.000000
        assert abs(gdf.describe()["elevation_start"]["min"] - 2882.457801) < 0.001
        assert abs(gdf.describe()["elevation_stop"]["min"] - 2738.054259) < 0.001
        assert abs(gdf.describe()["solar_elevation"]["min"] - 42.839184) < 0.001

    def test_cmr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        granules = earthdata.cmr(short_name="GEDI01_B", polygon=region["poly"])
        assert len(granules) >= 130
        assert 'GEDI01_B_' in granules[0]

@pytest.mark.network
class TestL2A:
    def test_gedi(self, domain, organization, desired_nodes):
        gedi.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        resource = "GEDI02_A_2022288203631_O21758_03_T00021_02_003_02_V002.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "degrade_flag": 0,
            "quality_flag": 1,
            "beam": 0
        }
        gdf = gedi.gedi02ap(parms, resources=[resource])
        assert gdf.describe()["beam"]["mean"] == 0.0
        assert gdf.describe()["flags"]["max"] == 130.0
        assert abs(gdf.describe()["elevation_lm"]["min"] - 667.862000) < 0.001
        assert abs(gdf.describe()["elevation_hr"]["min"] - 667.862000) < 0.001
        assert abs(gdf.describe()["sensitivity"]["min"] - 0.785598) < 0.001
        assert abs(gdf.describe()["solar_elevation"]["min"] - 30.522356) < 0.001

    def test_cmr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        granules = earthdata.cmr(short_name="GEDI02_A", polygon=region["poly"])
        assert len(granules) >= 130
        assert 'GEDI02_A_' in granules[0]

@pytest.mark.network
class TestL3:
    def test_gedi(self, domain, asset, organization, desired_nodes):
        gedi.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        resource = "ATL03_20220105023009_02111406_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region['poly'],
            "cnf": "atl03_high",
            "ats": 5.0,
            "cnt": 5,
            "len": 20.0,
            "res": 10.0,
            "maxi": 1,
            "track": 1,
            "samples": {"gedi": {"asset": "gedil3-elevation"}}
        }
        gdf = icesat2.atl06p(parms, asset=asset, resources=[resource])
        assert gdf.describe()["gedi.time"]["std"] == 0.0
        assert abs(gdf.describe()["gedi.value"]["mean"] - 3143.705389) < 0.001
        assert gdf.describe()["gedi.file_id"]["max"] == 0.0
        assert gdf.describe()["gedi.flags"]["max"] == 0.0

@pytest.mark.network
class TestL4A:
    def test_gedi(self, domain, organization, desired_nodes):
        gedi.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        resource = "GEDI04_A_2019123154305_O02202_03_T00174_02_002_02_V002.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region["poly"],
            "degrade_flag": 0,
            "l2_quality_flag": 1,
            "beam": 0
        }
        gdf = gedi.gedi04ap(parms, resources=[resource])
        assert gdf.describe()["beam"]["mean"] == 0.0
        assert gdf.describe()["flags"]["max"] == 134.0
        assert abs(gdf.describe()["elevation"]["min"] - 1499.137329) < 0.001
        assert abs(gdf.describe()["agbd"]["min"] - 0.919862) < 0.001
        assert abs(gdf.describe()["sensitivity"]["min"] - 0.900090) < 0.001
        assert abs(gdf.describe()["solar_elevation"]["min"] - 49.353821) < 0.001

    def test_cmr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        granules = earthdata.cmr(short_name="GEDI_L4A_AGB_Density_V2_1_2056", polygon=region["poly"])
        assert len(granules) >= 130
        assert 'GEDI04_A_' in granules[0]

@pytest.mark.network
class TestL4B:
    def test_gedi(self, domain, asset, organization, desired_nodes):
        gedi.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        resource = "ATL03_20220105023009_02111406_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly": region['poly'],
            "srt": icesat2.SRT_LAND,
            "len": 100,
            "res": 100,
            "track": 1,
            "pass_invalid": True,
            "atl08_class": ["atl08_ground", "atl08_canopy", "atl08_top_of_canopy"],
            "phoreal": {"binsize": 1.0, "geoloc": "center", "use_abs_h": False, "send_waveform": False},
            "samples": {"gedi": {"asset": "gedil4b"}}
        }
        gdf = icesat2.atl08p(parms, asset=asset, resources=[resource], keep_id=True)
        exp_keys = ['snowcover', 'h_max_canopy', 'canopy_h_metrics', 'solar_elevation',
                    'h_canopy', 'spot', 'segment_id', 'rgt', 'gnd_ph_count', 'h_te_median',
                    'cycle', 'ph_count', 'h_mean_canopy', 'distance', 'landcover',
                    'veg_ph_count', 'h_min_canopy', 'canopy_openness', 'gt', 'extent_id',
                    'geometry', 'gedi.flags', 'gedi.time', 'gedi.value', 'gedi.file_id']
        for key in exp_keys:
            assert key in gdf.keys()
        assert abs(gdf.describe()["canopy_openness"]["max"] - 10.390829) < 0.001
        df = gdf[gdf["gedi.value"] > -9999.0]
        assert abs(sum(df["gedi.value"]) - 31148.828197479248) < 0.001
