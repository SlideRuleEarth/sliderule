"""Tests for atl03x"""

import os
import pytest
import numpy as np
from pathlib import Path
from datetime import datetime
from sliderule import sliderule, icesat2
import geopandas as gpd

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
        assert len(gdf) == 429954
        assert 4 in np.unique(gdf.spot)

    def test_podppd_degrade_good(self, init):
        gdf_all = sliderule.run("atl03x", {"beams": "gt1l", "podppd": 0xFF}, AOI, RESOURCES)
        gdf_good = sliderule.run("atl03x", {"beams": "gt1l"}, AOI, RESOURCES)
        gdf_bad = sliderule.run("atl03x", {"beams": "gt1l", "podppd": 0xFE}, AOI, RESOURCES)
        gdf_no_filter = sliderule.run("atl03x", {"beams": "gt1l", "podppd": 0x00}, AOI, RESOURCES)
        assert init
        assert len(gdf_all) > 0
        assert len(gdf_good) == len(gdf_all)
        assert len(gdf_bad) == 0
        assert len(gdf_no_filter) == len(gdf_all)

    def test_podppd_degrade_bad(self, init):
        resource = "ATL03_20240420135502_05032302_006_01.h5"
        gdf_all = sliderule.run("atl03x", {"beams": "gt1l", "cnf": 4, "podppd": 0xFF}, resources=[resource])
        gdf_good = sliderule.run("atl03x", {"beams": "gt1l", "cnf": 4}, resources=[resource])
        gdf_bad = sliderule.run("atl03x", {"beams": "gt1l", "cnf": 4, "podppd": 0xFE}, resources=[resource])
        gdf_no_filter = sliderule.run("atl03x", {"beams": "gt1l", "cnf": 4, "podppd": 0x00}, resources=[resource])
        assert init
        assert len(gdf_all) > 0
        assert len(gdf_good) == 0
        assert len(gdf_bad) == len(gdf_all)
        assert len(gdf_no_filter) == len(gdf_all)

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
        resource = "ATL03_20181017222812_02950102_007_01.h5"
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "track": 3,
            "cnf": 0,
            "phoreal": {},
            "atl08_fields": ["sigma_topo"]
        }
        gdf = sliderule.run("atl03x", parms, region["poly"], [resource])
        assert init
        assert len(gdf) == 630
        assert len(gdf.keys()) == 25
        assert gdf["gt"].sum() == 35580
        assert gdf["vegetation_photon_count"].sum() == 16932
        assert abs(gdf["canopy_openness"].mean() - 3.6688475608825684) < 0.000001, f'canopy_openness = {gdf["canopy_openness"].mean()}'
        assert abs(gdf["h_min_canopy"].mean() - 1.0815950632095337) < 0.000001, f'h_min_canopy = {gdf["h_min_canopy"].mean()}'
        assert abs(gdf["h_te_median"].mean() - 1715.3670654296875) < 0.0001, f'h_te_median = {gdf["h_te_median"].mean()}'
        assert abs(gdf["sigma_topo"].mean() - 0.453180570587699) < 0.0001, f'sigma_topo = {gdf["sigma_topo"].mean()}'

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
        assert len(gdf) == 49548
        assert len(gdf.keys()) == 18
        assert gdf["atl24_class"].value_counts()[41] == 49523
        assert abs(gdf["atl24_confidence"].max() - 0.8872309) < 0.0001, gdf["atl24_confidence"].max()

#
# Earthdata Cloud no longer hosts ATL03 release 006. This means the
# ATL24 product generated from it can no longer be correlated to it.
#
#    def test_earthdata_atl24(self, init):
#        parms = {
#            "cnf": 0,
#            "poly": [ { "lon": -71.635926, "lat": 41.351689 },
#                    { "lon": -71.637573, "lat": 41.356567 },
#                    { "lon": -71.693417, "lat": 41.341954 },
#                    { "lon": -71.703369, "lat": 41.335941 },
#                    { "lon": -71.635926, "lat": 41.351689 } ],
#            "track": 1,
#            "beams": [ 10 ],
#            "rgt": 179,
#            "cycle": 8,
#            "atl24": {
#                "class_ph": [ "unclassified", "bathymetry", "sea_surface" ]
#            },
#            "cmr": {
#                "version": '006'
#            },
#            "yapc": {
#                "version": 0,
#                "score": 0
#            }
#        }
#        gdf = sliderule.run("atl03x", parms)
#        assert init
#        assert len(gdf) == 1066
#        assert len(gdf.keys()) == 19
#        assert gdf["yapc_score"].max() == 254
#        assert gdf["yapc_score"].min() == 0
#        assert gdf["atl24_class"].value_counts()[40] == 4

    def test_polygon_track_pair(self, init):
        poly = [
            { "lat": -80.75, "lon": -70.00 },
            { "lat": -81.00, "lon": -70.00 },
            { "lat": -81.00, "lon": -65.00 },
            { "lat": -80.75, "lon": -65.00 },
            { "lat": -80.75, "lon": -70.00 },
        ]
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "poly": poly,
        }
        gdf = sliderule.run("atl03x", parms, resources=RESOURCES)
        assert init
        # Track pair check (gt1l + gt1r) using the nominal polygon
        assert len(gdf) == 488670
        spot_counts = gdf.spot.value_counts().to_dict()
        assert spot_counts.get(5, 0) == 386717
        assert spot_counts.get(6, 0) == 101953
        lat_min = gdf.geometry.y.min()
        lat_max = gdf.geometry.y.max()
        tol = 1e-2  # allow small expansion due to dataset coordinates
        assert lat_min >= min(p["lat"] for p in poly) - tol
        assert lat_max <= max(p["lat"] for p in poly) + tol

    def test_polygon_tiny_offtrack_returns_empty(self, init):
        # Tiny polygon well away from the granule track; should return zero rows if AOI masking works
        poly = [
            { "lat": 0.0, "lon": 0.0 },
            { "lat": 0.001, "lon": 0.0 },
            { "lat": 0.001, "lon": 0.001 },
            { "lat": 0.0, "lon": 0.001 },
            { "lat": 0.0, "lon": 0.0 },
        ]
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "poly": poly,
        }
        gdf = sliderule.run("atl03x", parms, resources=RESOURCES)
        assert init
        # Regression guard: AOI refactors must not leak data outside the polygon
        assert len(gdf) == 0

    def test_empty_poly(self, init):
        empty_poly = [
            {"lon": 0.0, "lat": 0.0},
            {"lon": 0.1, "lat": 0.0},
            {"lon": 0.1, "lat": 0.1},
            {"lon": 0.0, "lat": 0.1},
            {"lon": 0.0, "lat": 0.0},
        ]
        # Using the nominal-track parameters to avoid scanning the entire granule; an empty AOI on the full
        # dataset would take too long just to confirm zero rows.
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
        }
        gdf_empty = sliderule.run("atl03x", parms, empty_poly, resources=RESOURCES)
        assert init
        assert len(gdf_empty) == 0
        # Ensure no beam slipped through and columns are consistent
        assert gdf_empty.empty
        # Default output columns should remain stable (matches nominal test expectation)
        assert len(gdf_empty.columns) == 16
        assert gdf_empty["gt"].value_counts().empty

    def test_aoi_mask_alignment_track_pair(self, init):
        # Alignment check across the track pair to catch AOI mask/offset regressions
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "poly": AOI,
        }
        gdf = sliderule.run("atl03x", parms, resources=RESOURCES)
        assert init
        gdf = gdf.sort_values("ph_index").reset_index(drop=True)
        # Total row count should match the nominal track-pair baseline
        assert len(gdf) == 488670
        # Photon-derived columns should stay aligned and non-null
        for col in ["ph_index", "atl03_cnf", "geometry"]:
            assert len(gdf[col]) == len(gdf)
            assert gdf[col].isnull().sum() == 0
        assert gdf.ph_index.nunique() == len(gdf)

        sample = list(zip(gdf.ph_index.iloc[:20], gdf.atl03_cnf.iloc[:20], gdf.geometry.y.iloc[:20]))
        expected = [
            (722068, 4, -80.755388647042),
            (722069, 1, -80.75539481025555),
            (722070, 1, -80.75539481084313),
            (722071, 0, -80.75540100533962),
            (722072, 4, -80.75540097681643),
            (722073, 4, -80.75540097602799),
            (722074, 4, -80.75540097680276),
            (722075, 0, -80.75541326304908),
            (722076, 4, -80.7554194713626),
            (722077, 4, -80.75541947101154),
            (722078, 4, -80.75542563636772),
            (722079, 4, -80.75542563634947),
            (722080, 4, -80.75542563636304),
            (722081, 4, -80.75542563678194),
            (722082, 4, -80.75543180234568),
            (722083, 1, -80.75543179165159),
            (722084, 4, -80.75543180161971),
            (722085, 4, -80.75543796820105),
            (722086, 4, -80.75543796811675),
            (722087, 4, -80.75543796823739),
        ]
        assert sample == expected
        assert gdf.ph_index.is_monotonic_increasing

    def test_polygon_narrow_span_offsets(self, init):
        # Narrow AOI to capture a small non-zero subset; hardcoded to catch offset regressions
        # Tight box around a known photon from the nominal run (lon ~ -65.9572, lat ~ -80.75539)
        narrow_poly = [
            {"lat": -80.7556, "lon": -65.9580},
            {"lat": -80.7552, "lon": -65.9580},
            {"lat": -80.7552, "lon": -65.9570},
            {"lat": -80.7556, "lon": -65.9570},
            {"lat": -80.7556, "lon": -65.9580},
        ]
        parms = {
            "track": 1,
            "cnf": 0,
            "srt": 3,
            "poly": narrow_poly,
        }
        gdf = sliderule.run("atl03x", parms, resources=RESOURCES)
        assert init
        assert len(gdf) == 147
        assert gdf.ph_index.min() == 721993
        assert gdf.ph_index.max() == 722141
        assert gdf.spot.value_counts().to_dict() == {6: 147}
        assert gdf.ph_index.nunique() == len(gdf)
        assert gdf.ph_index.is_monotonic_increasing

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
        assert len(gdf) == 1774988
        assert len(gdf.keys()) == 4
        assert "height" in gdf.keys()
        assert "x_atc" in gdf.keys()
        assert "atl03_cnf" in gdf.keys()
        assert "geometry" in gdf.keys()
