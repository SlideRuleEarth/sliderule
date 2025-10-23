"""Tests for Sliderule LAS/LAZ export using the dataframe pipeline."""

import os

import laspy
import numpy as np
import pytest
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


def _extract_time_seconds(gdf):
    if "time_ns" in gdf.columns:
        return gdf["time_ns"].to_numpy(dtype=np.float64) * 1e-9
    index = gdf.index.to_numpy()
    if np.issubdtype(index.dtype, np.datetime64):
        return index.astype("datetime64[ns]").astype(np.int64).astype(np.float64) * 1e-9
    raise AssertionError("Unable to determine time column for comparison")


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

            gdf_lon = gdf.geometry.x.to_numpy()
            gdf_lat = gdf.geometry.y.to_numpy()
            gdf_height = gdf["height"].to_numpy()
            gdf_time = _extract_time_seconds(gdf)

            def load_points(path):
                with laspy.open(path) as reader:
                    header = reader.header
                    points = reader.read()
                coords = np.vstack((points.x, points.y, points.z)).T
                gps_time = points["gps_time"]
                return coords, gps_time, header

            las_coords, las_time, las_header = load_points(las_path)
            laz_coords, laz_time, laz_header = load_points(laz_path)

            assert las_coords.shape[0] == len(gdf_lon)
            assert laz_coords.shape[0] == len(gdf_lon)

            def quantize(values, scale, offset):
                if scale == 0:
                    return values
                return offset + np.round((values - offset) / scale) * scale

            gdf_las = np.column_stack((
                quantize(gdf_lon, las_header.x_scale, las_header.x_offset),
                quantize(gdf_lat, las_header.y_scale, las_header.y_offset),
                quantize(gdf_height, las_header.z_scale, las_header.z_offset),
                gdf_time
            ))
            gdf_laz = np.column_stack((
                quantize(gdf_lon, laz_header.x_scale, laz_header.x_offset),
                quantize(gdf_lat, laz_header.y_scale, laz_header.y_offset),
                quantize(gdf_height, laz_header.z_scale, laz_header.z_offset),
                gdf_time
            ))

            las_points = np.column_stack((las_coords[:, 0], las_coords[:, 1], las_coords[:, 2], las_time))
            laz_points = np.column_stack((laz_coords[:, 0], laz_coords[:, 1], laz_coords[:, 2], laz_time))

            # LAS and LAZ outputs may differ in record order due to PDAL’s internal writer chunking.
            # Sorting is required for deterministic value comparison.
            def sort_points(points):
                order = np.lexsort((points[:, 3], points[:, 2], points[:, 1], points[:, 0]))
                return points[order]

            gdf_las_sorted = sort_points(gdf_las)
            gdf_laz_sorted = sort_points(gdf_laz)
            las_sorted = sort_points(las_points)
            laz_sorted = sort_points(laz_points)

            np.testing.assert_allclose(las_sorted[:, 0], gdf_las_sorted[:, 0], rtol=0, atol=max(las_header.x_scale, 1e-9))
            np.testing.assert_allclose(las_sorted[:, 1], gdf_las_sorted[:, 1], rtol=0, atol=max(las_header.y_scale, 1e-9))
            np.testing.assert_allclose(las_sorted[:, 2], gdf_las_sorted[:, 2], rtol=0, atol=max(las_header.z_scale, 1e-6))
            np.testing.assert_allclose(las_sorted[:, 3], gdf_las_sorted[:, 3], rtol=0, atol=1e-6)

            np.testing.assert_allclose(laz_sorted[:, 0], gdf_laz_sorted[:, 0], rtol=0, atol=max(laz_header.x_scale, 1e-9))
            np.testing.assert_allclose(laz_sorted[:, 1], gdf_laz_sorted[:, 1], rtol=0, atol=max(laz_header.y_scale, 1e-9))
            np.testing.assert_allclose(laz_sorted[:, 2], gdf_laz_sorted[:, 2], rtol=0, atol=max(laz_header.z_scale, 1e-6))
            np.testing.assert_allclose(laz_sorted[:, 3], gdf_laz_sorted[:, 3], rtol=0, atol=1e-6)

            np.testing.assert_allclose(las_sorted[:, :3], laz_sorted[:, :3], rtol=0, atol=1e-9)
            np.testing.assert_allclose(las_sorted[:, 3], laz_sorted[:, 3], rtol=0, atol=1e-6)

            las_size = os.path.getsize(las_path)
            laz_size = os.path.getsize(laz_path)
            assert (laz_size * 10) < las_size, "LAZ output should be much smaller than LAS output"
        finally:
            for path in (las_path, laz_path, geoparquet_path):
                if os.path.exists(path):
                    os.remove(path)
