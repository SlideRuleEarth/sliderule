"""eests for atl08x"""

import pytest
import numpy as np
from sliderule import sliderule

GRANULE = "ATL08_20200307004141_10890603_007_01.h5"


class TestAtl08x:
    def test_nominal(self, init):
        parms = {
            "beams": "gt3r",
        }

        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert init
        assert len(gdf) == 10214

        # Verify core/terrain/canopy fields populate
        required_columns = [
            "segment_id_beg",
            "rgt",
            "segment_landcover",
            "segment_snowcover",
            "n_seg_ph",
            "solar_elevation",
            "terrain_slope",
            "n_te_photons",
            "h_te_uncertainty",
            "h_canopy",
            "h_canopy_uncertainty",
            "segment_cover",
            "n_ca_photons",
            "h_te_median",
            "h_max_canopy",
            "h_min_canopy",
            "h_mean_canopy",
            "canopy_openness",
            "canopy_h_metrics",
            "spot",
            "cycle",
            "region",
            "gt",
            "geometry",
        ]
        assert set(required_columns).issubset(set(gdf.columns))

        # Verify no ancillary fields populate by default
        assert "beam_azimuth" not in gdf.columns
        assert "segment_watermask" not in gdf.columns

        row = gdf.iloc[0]  # First row has complete core/terrain/canopy values
        assert row.geometry.geom_type == "Point"
        assert gdf.index.name == "time_ns"

        np.testing.assert_allclose(
            [
                row.name.value,  # time_ns as int64
                row.geometry.x,
                row.geometry.y,
                row["segment_id_beg"],
                row["segment_landcover"],
                row["segment_snowcover"],
                row["n_seg_ph"],
                row["solar_elevation"],
                row["terrain_slope"],
                row["n_te_photons"],
                row["h_te_uncertainty"],
                row["h_canopy"],
                row["h_canopy_uncertainty"],
                row["segment_cover"],
                row["n_ca_photons"],
                row["h_te_median"],
                row["h_max_canopy"],
                row["h_min_canopy"],
                row["h_mean_canopy"],
                row["canopy_openness"],
            ],
            [
                1583541719170012416,
                -145.39999389648438,
                60.644901275634766,
                337095,
                126,
                2,
                345,
                16.451,
                -0.004300970584154129,
                68,
                3.91519,
                14.0389404296875,
                4.948657512664795,
                61,
                124,
                266.374,
                14.923,
                0.527679,
                4.07945,
                3.68869,
            ],
            rtol=0,
            atol=1e-3,
        )

        # 18 elements expected in canopy_h_metrics
        np.testing.assert_allclose(
            row["canopy_h_metrics"],
            [
                0.89624,
                1.06802,
                1.14206,
                1.23428,
                1.47107,
                1.71057,
                1.8457,
                2.0553,
                2.39349,
                3.24564,
                3.58926,
                4.13397,
                4.69241,
                5.64832,
                6.8551,
                8.27255,
                10.655,
                12.1895,
            ],
            rtol=0,
            atol=1e-3,
        )

    def test_ancillary_as_scalar(self, init):
        parms = {
            "beams": "gt3r",
            "atl08_fields": ["beam_azimuth", "segment_watermask"],
        }

        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert init
        assert len(gdf) == 10214
        assert set(["beam_azimuth", "segment_watermask"]).issubset(set(gdf.columns))

        row = gdf.iloc[0]  # Verify ancillary fields populate when requested
        np.testing.assert_allclose(
            [
                row["beam_azimuth"],
                row["segment_watermask"],
            ],
            [
                -1.29266,
                0,
            ],
            rtol=0,
            atol=1e-5,
        )

    def test_ancillary_as_array(self, init):
        parms = {
            "beams": "gt3r",
            "atl08_fields": [
                "terrain/h_te_best_fit_20m",
                "canopy/h_canopy_20m",
            ],
        }

        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert init
        assert len(gdf) == 10214

        # These fields should remain nested arrays (N,5) when requested
        for column in [
            "terrain/h_te_best_fit_20m",
            "canopy/h_canopy_20m",
        ]:
            assert column in gdf.columns
            sample = gdf[column].iloc[0]
            assert isinstance(sample, (list, tuple, np.ndarray))
            assert len(sample) == 5

    def test_quality_filters(self, init):
        base = sliderule.run("atl08x", {"beams": "gt3r"}, resources=[GRANULE])
        assert init
        assert len(base) == 10214
        assert "te_quality_score" not in base.columns
        assert "can_quality_score" not in base.columns

        # te-only filter
        parms = {"beams": "gt3r", "phoreal": {"te_quality_filter": 70}}
        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert len(gdf) == 9837
        assert "te_quality_score" in gdf.columns
        assert "can_quality_score" not in gdf.columns
        assert gdf["te_quality_score"].min() >= 70

        # can-only filter
        parms = {"beams": "gt3r", "phoreal": {"can_quality_filter": 75}}
        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert len(gdf) == 6486
        assert "te_quality_score" not in gdf.columns
        assert "can_quality_score" in gdf.columns
        assert gdf["can_quality_score"].min() >= 75

        # both filters
        parms = {"beams": "gt3r", "phoreal": {"te_quality_filter": 70, "can_quality_filter": 75}}
        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert len(gdf) == 6412
        assert "te_quality_score" in gdf.columns
        assert "can_quality_score" in gdf.columns
        assert gdf["te_quality_score"].min() >= 70
        assert gdf["can_quality_score"].min() >= 75

        # zero thresholds (should match baseline but include scores)
        parms = {"beams": "gt3r", "phoreal": {"te_quality_filter": 0, "can_quality_filter": 0}}
        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert len(gdf) == 10214
        assert "te_quality_score" in gdf.columns
        assert "can_quality_score" in gdf.columns
