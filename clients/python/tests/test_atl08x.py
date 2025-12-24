"""Tests for atl08x"""

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
            "extent_id",
            "segment_id_beg",
            "segment_id_end",
            "rgt",
            "night_flag",
            "n_seg_ph",
            "solar_elevation",
            "solar_azimuth",
            "terrain_flg",
            "brightness_flag",
            "cloud_flag_atm",
            "h_te_best_fit",
            "h_te_interp",
            "terrain_slope",
            "n_te_photons",
            "te_quality_score",
            "h_te_uncertainty",
            "h_canopy",
            "h_canopy_abs",
            "h_canopy_uncertainty",
            "segment_cover",
            "n_ca_photons",
            "can_quality_score",
            "spot",
            "cycle",
            "region",
            "gt",
            "delta_time",
            "geometry",
        ]
        assert set(required_columns).issubset(set(gdf.columns))

        # Verify no ancillary fields populate by default
        assert "segment_landcover" not in gdf.columns
        assert "segment_snowcover" not in gdf.columns

        row = gdf.iloc[0]  # First row has complete core/terrain/canopy values
        assert row.geometry.geom_type == "Point"
        assert gdf.index.name == "time_ns"

        np.testing.assert_allclose(
            [
                row.name.value,  # time_ns as int64
                row.geometry.x,
                row.geometry.y,
                row["delta_time"],
                row["segment_id_beg"],
                row["segment_id_end"],
                row["rgt"],
                row["night_flag"],
                row["n_seg_ph"],
                row["solar_elevation"],
                row["solar_azimuth"],
                row["terrain_flg"],
                row["brightness_flag"],
                row["cloud_flag_atm"],
                row["h_te_best_fit"],
                row["h_te_interp"],
                row["terrain_slope"],
                row["n_te_photons"],
                row["te_quality_score"],
                row["h_te_uncertainty"],
                row["h_canopy"],
                row["h_canopy_abs"],
                row["h_canopy_uncertainty"],
                row["segment_cover"],
                row["n_ca_photons"],
                row["can_quality_score"],
            ],
            [
                1583541719170012416,
                -1224416537368982.5,
                1277031622441.275,
                6.877691917001235e7,
                337095,
                337099,
                1089,
                0,
                345,
                16.451,
                224.372,
                1,
                0,
                2,
                266.6724548339844,
                266.65447998046875,
                -0.004300970584154129,
                68,
                70,
                3.91519,
                14.0389404296875,
                280.7113952636719,
                4.948657512664795,
                61,
                124,
                75,
            ],
            rtol=0,
            atol=1e-4,
        )

    def test_ancillary_as_scalar(self, init):
        parms = {
            "beams": "gt3r",
            "atl08_fields": ["segment_landcover", "segment_snowcover"],
        }

        gdf = sliderule.run("atl08x", parms, resources=[GRANULE])
        assert init
        assert len(gdf) == 10214
        assert set(["segment_landcover", "segment_snowcover"]).issubset(set(gdf.columns))

        row = gdf.iloc[0]  # Verify ancillary fields populate when requested
        np.testing.assert_array_equal(
            [
                row["segment_landcover"],
                row["segment_snowcover"],
            ],
            [
                126,
                2,
            ],
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
