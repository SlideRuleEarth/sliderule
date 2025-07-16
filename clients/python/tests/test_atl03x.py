"""Tests for sliderule icesat2 atl06-sr algorithm."""

import os
import pytest
import numpy as np
from pathlib import Path
from datetime import datetime
from sliderule import sliderule, icesat2

TESTDIR = Path(__file__).parent
RESOURCES = ["ATL03_20181019065445_03150111_006_02.h5"]
AOI = [ { "lat": -80.75, "lon": -70.00 },
        { "lat": -81.00, "lon": -70.00 },
        { "lat": -81.00, "lon": -65.00 },
        { "lat": -80.75, "lon": -65.00 },
        { "lat": -80.75, "lon": -70.00 } ]

class TestAtl03x:
    def test_nominal(self, init):
        parms = { "track": 1,
                  "cnf": 0,
                  "srt": 3 }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 488670
        assert len(gdf.keys()) == 16
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
        assert len(gdf.keys()) == 18
        assert gdf.spot.value_counts()[5] == 386717
        assert gdf.spot.value_counts()[6] == 101953
        assert gdf.cycle.describe()["mean"] == 1.0
        assert gdf.atl03_cnf.value_counts()[1] == 46768

    def test_spot_beam_filter(self, init):
        poly = [{'lat': -68.36177715438129, 'lon': 151.9541166015127},
                {'lat': -68.58236183989223, 'lon': 151.8438344989098},
                {'lat': -68.52516138552262, 'lon': 151.0200874351817},
                {'lat': -68.69420600399343, 'lon': 151.0186339046389},
                {'lat': -68.73791835254544, 'lon': 151.5665007283068},
                {'lat': -68.70011166045259, 'lon': 151.972856443959},
                {'lat': -68.80666559883542, 'lon': 152.7456805072033},
                {'lat': -68.33401220826397, 'lon': 152.7400123843097},
                {'lat': -68.36177715438129, 'lon': 151.9541166015127}]
        parms= {
            "poly":poly,
            "t0": "2019-12-02T01:00:00Z",
            "t1": "2020-01-30T17:58:49Z",
            "rgt":1320,
            "srt":icesat2.SRT_LAND_ICE,
            "cnf":-2,
            "beams": "gt2l"
        }
        gdf = sliderule.run("atl03x", parms)
        assert init
        assert len(gdf) == 430618
        assert 4 in np.unique(gdf.spot)

    def test_fitter(self, init):
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "fit": {"maxi": 3}
        }
        gdf = sliderule.run("atl03x", parms, AOI, RESOURCES)
        assert init
        assert len(gdf) == 2675
        assert len(gdf.keys()) == 17
        assert gdf["gt"].sum() == 40520
        assert abs(gdf["rms_misfit"].mean() - 0.22276999056339264) < 0.0001

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
        assert len(gdf) == 2873
        assert len(gdf.keys()) == 24
        assert gdf["ph_id_channel"].value_counts()[57.0] == 237
        assert gdf["pce_mframe_cnt"].max() == 110610389.0
        assert abs(gdf["ref_elev"].mean() - 1.5656124586346465) < 0.0001, f'{gdf["ref_elev"].mean()}'
        assert abs(gdf["ref_azimuth"].mean() - -2.077972882456559) < 0.0001, f'{gdf["ref_azimuth"].mean()}'
        assert abs(gdf["range_bias_corr"].mean() - 4.0305766004961745) < 0.0001, f'{gdf["range_bias_corr"].mean()}'
        assert abs(gdf["geoid"].mean() - -24.62919263010919) < 0.0001, f'{gdf["geoid"].mean()}'
        assert abs(gdf["sigma_topo"].mean() - 0.003645051751679172) < 0.0001, f'{gdf["sigma_topo"].mean()}'

    def test_phoreal(self, init):
        resource = "ATL03_20181017222812_02950102_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "track": 3,
            "cnf": 0,
            "phoreal": {},
            "atl08_fields": ["sigma_topo"]
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 774
        assert len(gdf.keys()) == 25
        assert gdf["gt"].sum() == 43210
        assert gdf["vegetation_photon_count"].sum() == 23127
        assert abs(gdf["canopy_openness"].mean() - 3.8784506) < 0.000001
        assert abs(gdf["h_min_canopy"].mean() - 1.6553754) < 0.000001
        assert abs(gdf["h_te_median"].mean() - 1487.2678) < 0.0001
        assert abs(gdf["sigma_topo"].mean() - 0.430706130944393362) < 0.0001, f'{gdf["sigma_topo"].mean()}'

    def test_sampler(self, init):
        resource = "ATL03_20190314093716_11600203_005_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "dicksonfjord.geojson"))
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "fit": {"maxi": 3},
            "samples": {"mosaic": {"asset": "arcticdem-mosaic", "force_single_sample": True}}
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 199
        assert len(gdf.keys()) == 20
        assert gdf["cycle"].mean() == 2
        assert abs(gdf["mosaic.value"].mean() - 1496.4541849874372) < 0.0001
        assert gdf["mosaic.fileid"].mean() == 0
        assert gdf["mosaic.time_ns"].iloc[0] == datetime.strptime('2023-01-18 20:23:42', '%Y-%m-%d %H:%M:%S')

    def test_atl03_sampler(self, init):
        parms = {
            "asset": "icesat2",
            "poly": [
                {"lat": 59.86856921063384, "lon": -44.34985645709006},
                {"lat": 59.85613150141896, "lon": -44.34985645709006},
                {"lat": 59.85613150141896, "lon": -44.30692727565953},
                {"lat": 59.86856921063384, "lon": -44.30692727565953},
                {"lat": 59.86856921063384, "lon": -44.34985645709006}
            ],
            "samples": {
                "mosaic": {
                    "asset": "arcticdem-mosaic",
                    "algorithm": "NearestNeighbour",
                    "force_single_sample": True
                }
            }
        }
        gdf = sliderule.run("atl03x", parms, resources=["ATL03_20190105024336_01170205_006_02.h5"])
        assert init
        assert len(gdf) == 527
        assert len(gdf.keys()) == 19
        assert "mosaic.value" in gdf

    def test_mixed_empty_beams(self, init):
        resource = 'ATL03_20200512071854_07140706_006_01.h5'
        grand_mesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                       {"lon": -107.7677425431139, "lat": 38.90611184543033},
                       {"lon": -107.7818591266989, "lat": 39.26613714985466},
                       {"lon": -108.3605610678553, "lat": 39.25086131372244},
                       {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        parms = {
            "poly": grand_mesa,
            "track": 2,
            "cnf": 0,
            "fit": {}
        }
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert init
        assert len(gdf) == 3
        assert len(gdf.keys()) == 17

    def test_atl24(self, init):
        resource = "ATL03_20181014001920_02350103_006_02.h5"
        parms = {
            "track": 1,
            "cnf": 0,
            "atl24": {"class_ph": ["bathymetry", "sea_surface"]}
        }
        gdf = sliderule.run("atl03x", parms, resources=[resource])
        assert init
        assert len(gdf) == 49554
        assert len(gdf.keys()) == 18
        assert gdf["atl24_class"].value_counts()[41] == 49523
        assert abs(gdf["atl24_confidence"].max() - 0.8872309) < 0.0001, gdf["atl24_confidence"].max()

    def test_earthdata_atl24(self, init):
        parms = {
            "cnf": 0,
            "poly": [ { "lon": -71.635926, "lat": 41.351689 },
                    { "lon": -71.637573, "lat": 41.356567 },
                    { "lon": -71.693417, "lat": 41.341954 },
                    { "lon": -71.703369, "lat": 41.335941 },
                    { "lon": -71.635926, "lat": 41.351689 } ],
            "track": 1,
            "beams": [ 10 ],
            "rgt": 179,
            "cycle": 8,
            "atl24": {
                "class_ph": [ "unclassified", "bathymetry", "sea_surface" ]
            },
            "yapc": {
                "version": 0,
                "score": 0
            }
        }
        gdf = sliderule.run("atl03x", parms)
        assert init
        assert len(gdf) == 1066
        assert len(gdf.keys()) == 19
        assert gdf["yapc_score"].max() == 254
        assert gdf["yapc_score"].min() == 0
        assert gdf["atl24_class"].value_counts()[40] == 4

    def test_datum(self, init):
        resource = "ATL03_20181014001920_02350103_006_02.h5"
        gdf1 = sliderule.run("atl03x", {"track": 1, "cnf": 4, "datum": "ITRF2014"}, resources=[resource])
        gdf2 = sliderule.run("atl03x", {"track": 1, "cnf": 4, "datum": "EGM08"}, resources=[resource])
        assert init
        assert len(gdf1) == len(gdf2)
        assert gdf1.height.mean() < gdf2.height.mean()

    def test_final_fields(self, init):
        parms = {
            "t0": "2019-12-02T01:00:00Z",
            "t1": "2020-01-30T17:58:49Z",
            "rgt":1322,
            "region": 9,
            "srt":icesat2.SRT_DYNAMIC,
            "cnf":icesat2.CNF_SURFACE_LOW,
            "spots": [3],
            "output":{
                "fields": ['height', 'x_atc', 'atl03_cnf']
            }
        }
        gdf = sliderule.run("atl03x", parms)
        assert init
        assert len(gdf) == 1774972
        assert len(gdf.keys()) == 4
        assert "height" in gdf.keys()
        assert "x_atc" in gdf.keys()
        assert "atl03_cnf" in gdf.keys()
        assert "geometry" in gdf.keys()
