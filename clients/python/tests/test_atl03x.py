"""Tests for sliderule icesat2 atl06-sr algorithm."""

import os
import pytest
from pathlib import Path
from sliderule import sliderule

TESTDIR = Path(__file__).parent
RESOURCES = ["ATL03_20181019065445_03150111_006_02.h5"]
AOI = [ { "lat": -80.75, "lon": -70.00 },
        { "lat": -81.00, "lon": -70.00 },
        { "lat": -81.00, "lon": -65.00 },
        { "lat": -80.75, "lon": -65.00 },
        { "lat": -80.75, "lon": -70.00 } ]

@pytest.mark.network
class TestAtl03x:

    def test_nominal(self, init):
        parms = { "track": 1,
                  "cnf": 0,
                  "srt": 3 }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 488670
        assert len(gdf.keys()) == 20
        assert gdf.spot.value_counts()[5] == 386717
        assert gdf.spot.value_counts()[6] == 101953
        assert gdf.cycle.describe()["mean"] == 1.0
        assert gdf.atl03_cnf.value_counts()[1] == 46768

    def test_yapc_atl08(self, init):
        parms = { "track": 1,
                  "cnf": 0,
                  "srt": 3,
                  "yapc": { "score": 0 },
                  "atl08_class": ["atl08_noise", "atl08_ground", "atl08_canopy", "atl08_top_of_canopy", "atl08_unclassified"] }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 488670
        assert len(gdf.keys()) == 20
        assert gdf.spot.value_counts()[5] == 386717
        assert gdf.spot.value_counts()[6] == 101953
        assert gdf.cycle.describe()["mean"] == 1.0
        assert gdf.atl03_cnf.value_counts()[1] == 46768

    def test_fitter(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "fit": {"maxi": 3}
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 2815
        assert len(gdf.keys()) == 16
        assert gdf["gt"].sum() == 42240
        assert abs(gdf["rms_misfit"].mean() - 0.2315002977848053) < 0.0001, f'{gdf["rms_misfit"].mean()}'

    def test_ancillary(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "fit": {"maxi": 3},
            "atl03_ph_fields": ["mode(ph_id_channel)", "median(pce_mframe_cnt)"],
            "atl03_geo_fields": ["ref_elev", "ref_azimuth", "range_bias_corr"],
            "atl03_corr_fields": ["geoid"],
            "atl08_fields": ["sigma_topo"]
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 2813
        assert len(gdf.keys()) == 23
        assert gdf["ph_id_channel"].value_counts()[57.0] == 232
        assert gdf["pce_mframe_cnt"].max() == 110610389.0
        assert abs(gdf["ref_elev"].mean() - 1.5656124586346465) < 0.0001, f'{gdf["ref_elev"].mean()}'
        assert abs(gdf["ref_azimuth"].mean() - -2.0788434097976025) < 0.0001, f'{gdf["ref_azimuth"].mean()}'
        assert abs(gdf["range_bias_corr"].mean() - 4.0305766004961745) < 0.0001, f'{gdf["range_bias_corr"].mean()}'
        assert abs(gdf["geoid"].mean() - -24.62919263010919) < 0.0001, f'{gdf["geoid"].mean()}'
        assert abs(gdf["sigma_topo"].mean() - 0.003645051751679172) < 0.0001, f'{gdf["sigma_topo"].mean()}'

    def test_phoreal(self, init):
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "track": 3,
            "cnf": 0,
            "phoreal": {},
            "atl08_fields": ["sigma_topo"]
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 774
        assert len(gdf.keys()) == 24
        assert gdf["gt"].sum() == 43210
        assert gdf["vegetation_photon_count"].sum() == 23127
        assert abs(gdf["canopy_openness"].mean() - 3.8784506) < 0.000001
        assert abs(gdf["h_min_canopy"].mean() - 1.6553754) < 0.000001
        assert abs(gdf["h_te_median"].mean() - 1487.2678) < 0.0001
        assert abs(gdf["sigma_topo"].mean() - 0.430706130944393362) < 0.0001, f'{gdf["sigma_topo"].mean()}'

    def test_sampler(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data/dicksonfjord.geojson"))
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "fit": {"maxi": 3},
            "samples": {"mosaic": {"asset": "arcticdem-mosaic", "force_single_sample": True}}
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 1279
        assert len(gdf.keys()) == 19
        assert gdf["cycle"].mean() == 2
        assert abs(gdf["mosaic.value"].mean() - 1498.9387766321345) < 0.0001
        assert gdf["mosaic.fileid"].mean() == 0
        assert gdf["mosaic.time"].mean() == 1358108640.0
