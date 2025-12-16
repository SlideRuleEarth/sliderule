"""Ancillary tests for atl03x"""

import numpy as np
import math
from pathlib import Path
from sliderule import sliderule

TESTDIR = Path(__file__).parent
RESOURCES = ["ATL03_20181019065445_03150111_006_02.h5"]
AOI = [
    {"lat": -80.75, "lon": -70.00},
    {"lat": -81.00, "lon": -70.00},
    {"lat": -81.00, "lon": -65.00},
    {"lat": -80.75, "lon": -65.00},
    {"lat": -80.75, "lon": -70.00},
]


class TestAtl03xAncillary:
    def test_ancillary_as_array(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "atl03_ph_fields": ["signal_conf_ph"]
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)

        assert init
        assert len(gdf) == 488670
        assert len(gdf.keys()) >= 17
        for column in [
            "signal_conf_ph",
        ]:
            assert column in gdf.keys()

        sample_signal_conf = gdf["signal_conf_ph"].iloc[0]
        assert isinstance(sample_signal_conf, (list, tuple, np.ndarray))
        assert len(sample_signal_conf) == 5
        for v in sample_signal_conf:
            assert v in (-2, -1, 0, 1, 2, 3, 4)
        assert all(len(v) == 5 for v in gdf["signal_conf_ph"].head(10))

    def test_ancillary_as_scalar(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "fit": {"maxi": 3},
            "atl03_ph_fields": ["signal_conf_ph", "mode(ph_id_channel)", "median(pce_mframe_cnt)"],
            "atl03_geo_fields": ["ref_elev", "ref_azimuth", "range_bias_corr"],
            "atl03_corr_fields": ["geoid"],
            "atl08_fields": ["sigma_topo"],
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)

        assert init
        assert len(gdf) == 2873
        assert len(gdf.keys()) >= 25
        for column in [
            "signal_conf_ph",
            "ph_id_channel",
            "pce_mframe_cnt",
            "ref_elev",
            "ref_azimuth",
            "range_bias_corr",
            "geoid",
            "sigma_topo",
        ]:
            assert column in gdf.keys()

        sample_signal_conf = gdf["signal_conf_ph"].iloc[0]
        median_pce_mframe_cnt = gdf["pce_mframe_cnt"].iloc[0]
        assert isinstance(sample_signal_conf, (float, np.floating))
        assert math.isnan(sample_signal_conf)
        assert isinstance(median_pce_mframe_cnt, (float, np.floating))
        assert median_pce_mframe_cnt == 110610167.0