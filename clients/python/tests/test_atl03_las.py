"""Tests for Sliderule LAS/LAZ export using dataframe pipeline."""

import os

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


@pytest.mark.usefixtures("init")
class TestAtl03Las:
    def test_atl03_las_laz(self, init):
        las_path = "test_atl03_las.las"
        laz_path = "test_atl03_las.laz"

        base_parms = {
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_HIGH,
            "ats": 10.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
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
            result_las = sliderule.run("atl03x", parms_las, AOI, RESOURCES)
            result_laz = sliderule.run("atl03x", parms_laz, AOI, RESOURCES)

            assert init
            assert isinstance(result_las, str) and result_las == las_path
            assert isinstance(result_laz, str) and result_laz == laz_path

            assert os.path.exists(las_path), "LAS file was not created"
            assert os.path.exists(laz_path), "LAZ file was not created"

            las_size = os.path.getsize(las_path)
            laz_size = os.path.getsize(laz_path)

            assert las_size > 0, "LAS file is empty"
            assert laz_size > 0, "LAZ file is empty"

            # LAZ file should be significantly smaller than LAS file
            assert laz_size * 10 < las_size, f"LAZ file ({laz_size}) should be smaller than LAS file ({las_size})"
        finally:
            print("Cleaning up test LAS/LAZ files...")
            if os.path.exists(las_path):
                os.remove(las_path)
            if os.path.exists(laz_path):
                os.remove(laz_path)
