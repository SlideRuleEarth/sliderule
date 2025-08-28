"""Tests datum support and CRS propagation through to raster sampling code."""

import os
import pytest
import numpy as np
from pathlib import Path
from datetime import datetime
from sliderule import sliderule, icesat2
import geopandas as gpd
from pyproj import CRS


poly = [
    {"lat": 59.86856921063384, "lon": -44.34985645709006},
    {"lat": 59.85613150141896, "lon": -44.34985645709006},
    {"lat": 59.85613150141896, "lon": -44.30692727565953},
    {"lat": 59.86856921063384, "lon": -44.30692727565953},
    {"lat": 59.86856921063384, "lon": -44.34985645709006},
]

resource = "ATL03_20190105024336_01170205_006_02.h5"

# Check that a GeoDataFrame is valid and can be reprojected
def assert_valid_geodataframe(gdf: gpd.GeoDataFrame):
    assert isinstance(gdf, gpd.GeoDataFrame), "Not a GeoDataFrame"

    # CRS must be parseable by pyproj
    assert gdf.crs is not None, "GeoDataFrame has no CRS"
    crs = CRS.from_user_input(gdf.crs)   # raises if invalid

    # Must have a geometry column
    assert gdf.geometry is not None, "GeoDataFrame has no geometry"
    assert not gdf.geometry.isna().all(), "Geometry column is entirely empty"

    # Basic operations should not fail
    # total bounds: requires valid geometry
    bounds = gdf.total_bounds
    assert len(bounds) == 4, "Total bounds not valid"

    # reproject (smoke test): should succeed and preserve row count
    gdf_reproj = gdf.head(5).to_crs(4326 if not crs.is_geographic else 3857)
    assert len(gdf_reproj) == min(len(gdf), 5), "Reprojection changed row count unexpectedly"
    assert gdf_reproj.crs is not None, "Reprojected GeoDataFrame lost CRS"

def check_offset(gdf_itrf, gdf_egm08):
    assert len(gdf_itrf) == len(gdf_egm08)
    assert len(gdf_itrf.keys()) == len(gdf_egm08.keys())
    assert "mosaic.value" in gdf_itrf or "strips.value" in gdf_itrf

    # Pick the column name dynamically
    col = "mosaic.value" if "mosaic.value" in gdf_itrf else "strips.value"

    v_itrf = np.asarray(gdf_itrf[col], dtype=float)
    v_egm  = np.asarray(gdf_egm08[col], dtype=float)
    dv     = v_itrf - v_egm

    # Offsets should be ~43 meters higher for ITRF vs EGM08 for the area used
    expected_offset = 43.0
    margin = 1.0

    # Check every offset is within expected ± margin
    assert np.all((dv > expected_offset - margin) & (dv < expected_offset + margin)), \
                  f"Offsets not within {margin} m of {expected_offset}: " f"min={dv.min()}, max={dv.max()}"


class TestAtl03x_Datum:
    def test_datum(self, init):
        def _squash_ws(s: str) -> str:
            # remove all whitespace characters (spaces, tabs, newlines, etc.)
            return ''.join(s.split())

        # Valid and currently supported datums
        gdf1 = sliderule.run("atl03x", {"track": 1, "cnf": 4, "datum": "ITRF2014"}, resources=["ATL03_20181014001920_02350103_006_02.h5"])
        gdf2 = sliderule.run("atl03x", {"track": 1, "cnf": 4, "datum": "EGM08"}, resources=["ATL03_20181014001920_02350103_006_02.h5"])

        assert init
        assert len(gdf1) == len(gdf2)
        assert gdf1.height.mean() < gdf2.height.mean()

        # Compare CRS against files (skip NAVD88)
        crs_dir = (Path(__file__).parent / "../../../datasets/icesat2/crsfiles").resolve()
        itrf_file_str  = _squash_ws((crs_dir / "EPSG7912.projjson").read_text(encoding="utf-8"))
        egm08_file_str = _squash_ws((crs_dir / "EPSG7912_EGM08.projjson").read_text(encoding="utf-8"))

        itrf_gdf_str  = _squash_ws(gdf1.crs.to_json())
        egm08_gdf_str = _squash_ws(gdf2.crs.to_json())

        assert itrf_file_str  == itrf_gdf_str
        assert egm08_file_str == egm08_gdf_str

        # Assert both are valid GeoDataFrames for geopandas
        assert_valid_geodataframe(gdf1)
        assert_valid_geodataframe(gdf2)

        # Invalid datum: expect an exception from the server
        # TODO: re-enable when github issue #234 is fixed
        # with pytest.raises(Exception):
        #     sliderule.run("atl03x", {"track": 1, "cnf": 4, "datum": "NOSUCH_DATUM"}, resources=[resource])

    def test_atl03_sampler_geo_raster(self, init):
        assert init

        parms_base = {
            "asset": "icesat2",
            "poly": poly,
            "samples": {"mosaic": {"asset": "arcticdem-mosaic", "algorithm": "NearestNeighbour", "with_flags": True}},
        }
        parms_itrf = dict(parms_base); parms_itrf["datum"] = "ITRF2014"
        parms_egm  = dict(parms_base); parms_egm["datum"]  = "EGM08"

        gdf_itrf = sliderule.run("atl03x", parms_itrf, resources=[resource])
        gdf_egm  = sliderule.run("atl03x", parms_egm,  resources=[resource])

        # Both runs should return the same structure (527 rows, 20 columns)
        assert len(gdf_itrf) == 527 and len(gdf_egm) == 527
        assert len(gdf_itrf.keys()) == 20 and len(gdf_egm.keys()) == 20
        check_offset(gdf_itrf, gdf_egm)

    def test_atl03_sampler_geo_index_raster(self, init):
        assert init

        parms_base = {
            "asset": "icesat2",
            "poly": poly,
            "samples": {"strips": {"asset": "arcticdem-strips", "algorithm": "NearestNeighbour", "closest_time": "2021:2:4:23:3:0", "zonal_stats": True}},
        }
        parms_itrf = dict(parms_base); parms_itrf["datum"] = "ITRF2014"
        parms_egm  = dict(parms_base); parms_egm["datum"]  = "EGM08"

        gdf_itrf = sliderule.run("atl03x", parms_itrf, resources=[resource])
        gdf_egm  = sliderule.run("atl03x", parms_egm,  resources=[resource])

        # Both runs should return the same structure (527 rows, 26 columns)
        assert len(gdf_itrf) == 527 and len(gdf_egm) == 527
        assert len(gdf_itrf.keys()) == 26 and len(gdf_egm.keys()) == 26
        check_offset(gdf_itrf, gdf_egm)