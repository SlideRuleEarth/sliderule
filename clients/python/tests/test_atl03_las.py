"""Tests for Sliderule LAS/LAZ export using the dataframe pipeline."""

import os
from contextlib import contextmanager

import numpy as np
import pytest
import geopandas
from pyproj import CRS
from sliderule import sliderule, icesat2, las



RESOURCES = ["ATL03_20181017222812_02950102_007_01.h5"]
AOI = [
    {"lon": -108.20772968780051, "lat": 38.8232055291981},
    {"lon": -108.07460164311031, "lat": 38.8475137825863},
    {"lon": -107.72839858755752, "lat": 39.01510930230633},
    {"lon": -107.78724142490994, "lat": 39.195630349659986},
    {"lon": -108.17287000970857, "lat": 39.15920066396116},
    {"lon": -108.31168256553767, "lat": 39.13757646212944},
    {"lon": -108.34115668325224, "lat": 39.03758987613325},
    {"lon": -108.2878686387796, "lat": 38.89051431295789},
    {"lon": -108.20772968780051, "lat": 38.8232055291981},
]


def _base_parms():
    return {
        "srt": icesat2.SRT_LAND,
        "cnf": icesat2.CNF_SURFACE_HIGH,
        "ats": 10.0,
        "cnt": 10,
        "len": 40.0,
        "res": 20.0,
    }


def _output_parms(base, path, fmt, open_on_complete):
    parms = dict(base)
    parms["output"] = {
        "path": path,
        "format": fmt,
        "open_on_complete": open_on_complete,
    }
    return parms


@contextmanager
def _atl03_test_run(open_on_complete_outputs, *, suffix):
    las_path = f"test_atl03_output{suffix}.las"
    laz_path = f"test_atl03_output{suffix}.laz"
    geoparquet_path = f"test_atl03_output{suffix}.parquet"

    base = _base_parms()
    parms_df = _output_parms(base, geoparquet_path, "geoparquet", True)
    parms_las = _output_parms(base, las_path, "las", open_on_complete_outputs)
    parms_laz = _output_parms(base, laz_path, "laz", open_on_complete_outputs)

    try:
        gdf = sliderule.run("atl03x", parms_df, AOI, RESOURCES)
        las_result = sliderule.run("atl03x", parms_las, AOI, RESOURCES)
        laz_result = sliderule.run("atl03x", parms_laz, AOI, RESOURCES)
        yield {
            "gdf": gdf,
            "las": las_result,
            "laz": laz_result,
            "paths": (las_path, laz_path, geoparquet_path),
        }
    finally:
        for path in (las_path, laz_path, geoparquet_path):
            if os.path.exists(path):
                os.remove(path)


def _gps_time(gdf):
    for column in gdf.columns:
        if column.lower() in ("gpstime", "gps_time"):
            return gdf[column].to_numpy(dtype=float, copy=False)
    pytest.fail("GeoDataFrame missing GpsTime column")


def _to_crs(value):
    if value is None:
        return None
    if isinstance(value, CRS):
        return value
    try:
        return CRS.from_user_input(value)
    except Exception:
        return None


def _extract_gdf_coords(gdf):
    if len(gdf) == 0:
        return np.empty((0, 3), dtype=float)
    geometry = gdf.geometry
    x = geometry.x.to_numpy(dtype=float)
    y = geometry.y.to_numpy(dtype=float)
    if "height" in gdf:
        z = gdf["height"].to_numpy(dtype=float)
    else:
        z = np.array(
            [
                geom.z if getattr(geom, "has_z", False) else np.nan
                for geom in geometry
            ],
            dtype=float,
        )
        if np.isnan(z).any():
            pytest.fail("GeoDataFrame missing height column and geometry is not 3D")
    return np.column_stack((x, y, z))


def _sorted_coords(coords, time=None, decimals=8):
    coords = np.asarray(coords, dtype=np.float64)
    if coords.size == 0:
        if time is None:
            return coords
        return coords, np.asarray(time, dtype=np.float64)
    coords = np.round(coords, decimals=decimals)
    if time is None:
        structured = np.empty(coords.shape[0], dtype=[("x", float), ("y", float), ("z", float)])
        structured["x"] = coords[:, 0]
        structured["y"] = coords[:, 1]
        structured["z"] = coords[:, 2]
        order = np.argsort(structured, order=("x", "y", "z"))
        return coords[order]
    time_arr = np.round(np.asarray(time, dtype=np.float64), decimals=decimals)
    structured = np.empty(
        coords.shape[0],
        dtype=[("x", float), ("y", float), ("z", float), ("t", float)],
    )
    structured["x"] = coords[:, 0]
    structured["y"] = coords[:, 1]
    structured["z"] = coords[:, 2]
    structured["t"] = time_arr
    order = np.argsort(structured, order=("x", "y", "z", "t"))
    return coords[order], time_arr[order]


# -------------------------------------------------------------------------
# Helpers to Eliminate Repetition
# -------------------------------------------------------------------------

def _check_output_file(path):
    assert os.path.exists(path), f"Output file missing: {path}"
    assert os.path.getsize(path) > 0, f"Output file empty: {path}"


def _load_las_laz(las_path, laz_path):
    las_gdf = las.load(las_path)
    laz_gdf = las.load(laz_path)
    return las_gdf, laz_gdf


def _compare_coords_and_time(gdf, las_gdf, laz_gdf):
    gdf_coords = _extract_gdf_coords(gdf)
    las_coords = _extract_gdf_coords(las_gdf)
    laz_coords = _extract_gdf_coords(laz_gdf)

    las_time = _gps_time(las_gdf)
    laz_time = _gps_time(laz_gdf)

    gdf_coords_sorted = _sorted_coords(gdf_coords)
    las_coords_sorted, las_time_sorted = _sorted_coords(las_coords, las_time)
    laz_coords_sorted, laz_time_sorted = _sorted_coords(laz_coords, laz_time)

    np.testing.assert_allclose(
        las_coords_sorted, gdf_coords_sorted, rtol=0.0, atol=3.2e-3, equal_nan=True
    )
    np.testing.assert_allclose(
        laz_coords_sorted, gdf_coords_sorted, rtol=0.0, atol=3.2e-3, equal_nan=True
    )
    np.testing.assert_allclose(
        laz_coords_sorted, las_coords_sorted, rtol=0.0, atol=3.2e-3, equal_nan=True
    )

    np.testing.assert_allclose(
        laz_time_sorted, las_time_sorted, rtol=0.0, atol=1e-9, equal_nan=True
    )


def _compare_crs(gdf, las_gdf, laz_gdf):
    gdf_crs_raw = gdf.crs

    # Normalize CRS objects for comparison
    norm_gdf_crs = _to_crs(gdf_crs_raw)
    norm_las_crs = las_gdf.crs
    norm_laz_crs = laz_gdf.crs

    # Horizontal CRS
    assert norm_gdf_crs.to_2d() == norm_las_crs.to_2d() == norm_laz_crs.to_2d()

    # Vertical CRS
    def _vertical(crs):
        comps = getattr(crs, "sub_crs_list", None)
        if comps:
            for comp in comps:
                if comp.is_vertical:
                    return comp
        return None

    gdf_vert = _vertical(norm_gdf_crs)
    las_vert = _vertical(norm_las_crs)
    laz_vert = _vertical(norm_laz_crs)

    assert las_vert == laz_vert == gdf_vert


def _compare_compression(las_path, laz_path):
    las_size = os.path.getsize(las_path)
    laz_size = os.path.getsize(laz_path)
    ratio = las_size / laz_size
    assert ratio > 4.0, f"Unexpectedly low compression ratio: {ratio:.2f}"


# -------------------------------------------------------------------------
# Tests
# -------------------------------------------------------------------------

@pytest.mark.usefixtures("init")
class TestAtl03Las:

    @pytest.mark.parametrize("open_on_complete", [False, True])
    def test_open_on_complete(self, init, open_on_complete):
        suffix = "" if not open_on_complete else "_gdf"

        with _atl03_test_run(open_on_complete, suffix=suffix) as run:
            gdf = run["gdf"]
            las_path, laz_path, _ = run["paths"]

            assert init
            assert len(gdf) > 0

            _check_output_file(las_path)
            _check_output_file(laz_path)

            # Load LAS and LAZ based on mode
            if open_on_complete:
                # Sliderule already returned GeoDataFrames
                las_gdf = run["las"]
                laz_gdf = run["laz"]
                assert isinstance(las_gdf, geopandas.GeoDataFrame)
                assert isinstance(laz_gdf, geopandas.GeoDataFrame)
            else:
                # Sliderule returned paths, so load manually
                las_gdf, laz_gdf = _load_las_laz(las_path, laz_path)

            # Lengths must match ground truth
            assert len(las_gdf) == len(gdf)
            assert len(laz_gdf) == len(gdf)

            # Shared comparisons
            _compare_coords_and_time(gdf, las_gdf, laz_gdf)
            _compare_crs(gdf, las_gdf, laz_gdf)
            _compare_compression(las_path, laz_path)
