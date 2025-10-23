"""Tests for Sliderule LAS/LAZ export using the dataframe pipeline."""

import os

import json
import subprocess

import laspy
import numpy as np
import pytest
from pyproj import CRS
from sliderule import sliderule, icesat2


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


def _pdal_crs(path):
    try:
        proc = subprocess.run(
            ["pdal", "info", path, "--metadata"],
            capture_output=True,
            text=True,
            check=True,
        )
    except FileNotFoundError:
        pytest.skip("pdal command-line tool is not available on this system")
    except subprocess.CalledProcessError as exc:
        pytest.fail(f"pdal info failed for {path}: {exc.stderr.strip()}")

    metadata = json.loads(proc.stdout).get("metadata", {})
    return metadata.get("comp_spatialreference") or metadata.get("spatialreference")


def _to_crs(value):
    if value is None:
        return None
    if isinstance(value, CRS):
        return value
    try:
        return CRS.from_user_input(value)
    except Exception:
        return None


@pytest.mark.usefixtures("init")
class TestAtl03Las:
    def test_atl03_las_laz_match_dataframe(self, init):
        las_path = "test_atl03_output.las"
        laz_path = "test_atl03_output.laz"
        geoparquet_path = "test_atl03_output.parquet"

        base_parms = {
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_HIGH,
            "ats": 10.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
        }

        parms_df = dict(base_parms)
        parms_df["output"] = {
            "path": geoparquet_path,
            "format": "geoparquet",
            "open_on_complete": True,
        }

        parms_las = dict(base_parms)
        parms_las["output"] = {
            "path": las_path,
            "format": "las",
            "open_on_complete": False,
        }

        parms_laz = dict(base_parms)
        parms_laz["output"] = {
            "path": laz_path,
            "format": "laz",
            "open_on_complete": False,
        }

        try:
            gdf = sliderule.run("atl03x", parms_df, AOI, RESOURCES)
            result_las = sliderule.run("atl03x", parms_las, AOI, RESOURCES)
            result_laz = sliderule.run("atl03x", parms_laz, AOI, RESOURCES)

            assert init
            assert len(gdf) > 0
            for result, path in ((result_las, las_path), (result_laz, laz_path)):
                assert isinstance(result, str) and result == path
                assert os.path.exists(path), f"Output file missing: {path}"
                assert os.path.getsize(path) > 0, f"Output file empty: {path}"

            def load_points(path):
                with laspy.open(path) as reader:
                    header = reader.header
                    points = reader.read()
                coords = np.vstack((points.x, points.y, points.z)).T
                gps_time = points["gps_time"]
                return coords, gps_time, header

            las_coords, las_time, las_header = load_points(las_path)
            laz_coords, laz_time, laz_header = load_points(laz_path)

            assert las_coords.shape[0] == len(gdf)
            assert laz_coords.shape[0] == len(gdf)

            gdf_crs_raw = getattr(gdf, "crs", None)

            las_crs_raw = _pdal_crs(las_path)
            laz_crs_raw = _pdal_crs(laz_path)

            assert len(str(gdf_crs_raw)) > 0, "Missing CRS in GeoDataFrame"
            assert len(las_crs_raw) > 0, "Missing CRS in PDAL LAS output"
            assert len(laz_crs_raw) > 0, "Missing CRS in PDAL LAZ output"
            assert str(las_crs_raw) == str(laz_crs_raw), "CRS mismatch between LAS and LAZ outputs"

            # Normalize CRS for comparison, accounting for different representations in las, laz, and GeoDataFrame
            norm_gdf_crs = _to_crs(gdf_crs_raw)
            norm_las_crs = _to_crs(las_crs_raw)
            norm_laz_crs = _to_crs(laz_crs_raw)

            print(f"GDF CRS: {gdf_crs_raw}")
            print(f"PDAL LAS CRS: {las_crs_raw}")
            print(f"PDAL LAZ CRS: {laz_crs_raw}")

            # Compare horizontal CRS components
            gdf_horiz = norm_gdf_crs.to_2d()
            las_horiz = norm_las_crs.to_2d()
            laz_horiz = norm_laz_crs.to_2d()
            assert las_horiz == laz_horiz == gdf_horiz, "Horizontal CRS mismatch between outputs"

            def _vertical(crs):
                try:
                    comps = getattr(crs, "sub_crs_list", None)
                    if comps:
                        for comp in comps:
                            if comp.is_vertical:
                                return comp
                except AttributeError:
                    pass
                return None

            # Compare vertical CRS components
            gdf_vert = _vertical(norm_gdf_crs)
            las_vert = _vertical(norm_las_crs)
            laz_vert = _vertical(norm_laz_crs)

            assert las_vert == laz_vert == gdf_vert, "Vertical CRS mismatch between outputs"

            las_size = os.path.getsize(las_path)
            laz_size = os.path.getsize(laz_path)
            assert (laz_size * 10) < las_size, "LAZ output should be much smaller than LAS output"
        finally:
            print("Cleaning up test output files...")
            for path in (las_path, laz_path, geoparquet_path):
                if os.path.exists(path):
                    os.remove(path)
