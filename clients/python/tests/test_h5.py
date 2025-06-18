"""Tests for h5 endpoint."""

import pytest
from sliderule import h5 as h5coro

ATL03_FILE1 = "ATL03_20181019065445_03150111_005_01.h5"
ATL03_FILE2 = "ATL03_20181016104402_02720106_005_01.h5"
ATL06_FILE1 = "ATL06_20181019065445_03150111_005_01.h5"
ATL06_FILE2 = "ATL06_20181110092841_06530106_005_01.h5"
INVALID_FILE = "ATL99_20032_2342.h5"

class TestApi:
    def test_happy_case(self, init):
        epoch_offset = h5coro.h5("ancillary_data/atlas_sdp_gps_epoch", ATL03_FILE1, "icesat2")[0]
        assert init
        assert epoch_offset == 1198800018.0

    def test_h5_types(self, init):
        heights_64 = h5coro.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE1, "icesat2")
        expected_64 = [45.95665, 45.999374, 46.017857, 46.015575, 46.067562, 46.099796, 46.14037, 46.105526, 46.096024, 46.12297]
        heights_32 = h5coro.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE2, "icesat2")
        expected_32 = [350.46988, 352.08688, 352.43243, 353.19345, 353.69543, 352.25998, 350.15366, 346.37888, 342.47903, 341.51]
        bckgrd_32nf = h5coro.h5("/gt1l/bckgrd_atlas/bckgrd_rate", ATL03_FILE2, "icesat2")
        expected_32nf = [29311.428, 6385.937, 6380.8413, 28678.951, 55349.168, 38201.082, 19083.434, 38045.67, 34942.434, 38096.266]
        assert init
        for c in zip(heights_64, expected_64, heights_32, expected_32, bckgrd_32nf, expected_32nf):
            assert (round(c[0]) == round(c[1])) and (round(c[2]) == round(c[3])) and (round(c[4]) == round(c[5]))

    def test_variable_length(self, init):
        v = h5coro.h5("/gt1r/geolocation/segment_ph_cnt", ATL03_FILE1, "icesat2")
        assert init
        assert v[0] == 258 and v[1] == 256 and v[2] == 273

    def test_invalid_file(self, init):
        v = h5coro.h5("/gt1r/geolocation/segment_ph_cnt", INVALID_FILE, "icesat2")
        assert init
        assert len(v) == 0

    def test_invalid_asset(self, init):
        v = h5coro.h5("/gt1r/geolocation/segment_ph_cnt", ATL03_FILE1, "invalid-asset")
        assert init
        assert len(v) == 0

    def test_invalid_path(self, init):
        v = h5coro.h5("/gt1r/invalid-path", ATL03_FILE1, "icesat2")
        assert init
        assert len(v) == 0
