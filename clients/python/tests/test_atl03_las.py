"""Tests for Sliderule LAS/LAZ export using the dataframe pipeline."""

import json
import os

import numpy as np
import pytest
import pdal
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


def _run_pdal_pipeline(path):
    pipeline_json = json.dumps(
        {
            "pipeline": [
                {
                    "type": "readers.las",
                    "filename": path,
                }
            ]
        }
    )
    pipeline = pdal.Pipeline(pipeline_json)
    try:
        if hasattr(pipeline, "validate"):
            pipeline.validate()
        pipeline.execute()
    except RuntimeError as exc:  # pragma: no cover - runtime failure should fail test
        pytest.fail(f"PDAL pipeline failed for {path}: {exc}")

    arrays = pipeline.arrays
    raw_metadata = pipeline.metadata
    if isinstance(raw_metadata, (bytes, bytearray)):
        raw_metadata = raw_metadata.decode("utf-8")
    if isinstance(raw_metadata, str):
        raw_metadata = json.loads(raw_metadata)
    metadata = (
        raw_metadata.get("metadata", {}).get("readers.las", {}) if isinstance(raw_metadata, dict) else {}
    )
    return arrays, metadata


def _load_points(path):
    arrays, metadata = _run_pdal_pipeline(path)
    if not arrays:
        return np.empty((0, 3)), np.array([], dtype=float), metadata

    data = arrays[0]
    coords = np.column_stack((data["X"], data["Y"], data["Z"]))
    gps_field = next((name for name in data.dtype.names if name.lower() == "gpstime"), None)
    if gps_field is None:
        pytest.fail("PDAL pipeline output missing GpsTime dimension")
    gps_time = data[gps_field]
    return coords, gps_time, metadata


def _pdal_crs(metadata):
    if not metadata:
        return None
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

            las_coords, las_time, las_metadata = _load_points(las_path)
            laz_coords, laz_time, laz_metadata = _load_points(laz_path)

            assert las_coords.shape[0] == len(gdf)
            assert laz_coords.shape[0] == len(gdf)

            gdf_crs_raw = getattr(gdf, "crs", None)
            gdf_coords = _extract_gdf_coords(gdf)

            # Sort all point clouds into a consistent XYZ order so we are comparing like-for-like
            # despite PDAL's ingestion ordering and the GeoDataFrame's time-index sorting.
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

            # las/laz time should match exactly since it is stored as double precision GPS Time in seconds
            np.testing.assert_allclose(
                laz_time_sorted, las_time_sorted, rtol=0.0, atol=1e-9, equal_nan=True
            )

            las_crs_raw = _pdal_crs(las_metadata)
            laz_crs_raw = _pdal_crs(laz_metadata)

            assert gdf_crs_raw is not None and str(gdf_crs_raw), "Missing CRS in GeoDataFrame"
            assert isinstance(las_crs_raw, str) and las_crs_raw.strip(), "Missing CRS in PDAL LAS output"
            assert isinstance(laz_crs_raw, str) and laz_crs_raw.strip(), "Missing CRS in PDAL LAZ output"
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
            compression_ratio = las_size / laz_size
            assert compression_ratio > 4.0, f"Unexpectedly low compression ratio: {compression_ratio:.2f}"


        finally:
            print("Cleaning up test output files...")
            for path in (las_path, laz_path, geoparquet_path):
                if os.path.exists(path):
                    os.remove(path)
