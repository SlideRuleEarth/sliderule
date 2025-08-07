"""Tests for h5 endpoint."""

import pytest
from sliderule import h5 as h5coro

ATL03_FILE1 = "ATL03_20181019065445_03150111_007_01.h5"
ATL03_FILE2 = "ATL03_20181016104402_02720106_007_01.h5"
ATL06_FILE1 = "ATL06_20181019065445_03150111_007_01.h5"
ATL06_FILE2 = "ATL06_20181110092841_06530106_007_01.h5"
INVALID_FILE = "ATL99_20032_2342.h5"

class TestApi:
    def test_happy_case(self, init):
        epoch_offset = h5coro.h5("ancillary_data/atlas_sdp_gps_epoch", ATL03_FILE1, "icesat2")[0]
        assert init
        assert epoch_offset == 1198800018.0

    def test_h5_types(self, init):
        heights_64 = h5coro.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE1, "icesat2")
        expected_64 = [45.918957, 45.95782, 45.989723, 45.982258, 46.03089, 46.07748, 46.092117, 46.06058, 46.052357, 46.087215]
        heights_32 = h5coro.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE2, "icesat2")
        expected_32 = [350.7689, 352.25482, 352.21744, 353.19803, 353.66928, 352.06366, 348.95926, 346.38608, 342.27283, 340.64496]
        bckgrd_32nf = h5coro.h5("/gt1l/bckgrd_atlas/bckgrd_rate", ATL03_FILE2, "icesat2")
        expected_32nf = [29311.883, 6385.616, 6380.865, 28678.928, 55348.918, 38200.426, 19083.5, 38045.562, 34942.902, 38095.367]
        assert init
        for c in zip(heights_64, expected_64, heights_32, expected_32, bckgrd_32nf, expected_32nf):
            assert (round(c[0]) == round(c[1])) and (round(c[2]) == round(c[3])) and (round(c[4]) == round(c[5]))

    def test_variable_length(self, init):
        v = h5coro.h5("/gt1r/geolocation/segment_ph_cnt", ATL03_FILE1, "icesat2")
        assert init
        assert v[0] == 234 and v[1] == 263 and v[2] == 282

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
