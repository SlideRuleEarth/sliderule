"""Tests for h5 endpoint."""

import pytest
import sliderule
from sliderule import icesat2

ATL03_FILE1 = "ATL03_20181019065445_03150111_004_01.h5"
ATL03_FILE2 = "ATL03_20181016104402_02720106_004_01.h5"
ATL06_FILE1 = "ATL06_20181019065445_03150111_004_01.h5"
ATL06_FILE2 = "ATL06_20181110092841_06530106_004_01.h5"
INVALID_FILE = "ATL99_20032_2342.h5"

@pytest.mark.network
class TestApi:
    def test_happy_case(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        epoch_offset = icesat2.h5("ancillary_data/atlas_sdp_gps_epoch", ATL03_FILE1, asset)[0]
        assert epoch_offset == 1198800018.0

    def test_h5_types(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        heights_64 = icesat2.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE1, asset)
        expected_64 = [45.95665, 45.999374, 46.017857, 46.015575, 46.067562, 46.099796, 46.14037, 46.105526, 46.096024, 46.12297]
        heights_32 = icesat2.h5("/gt1l/land_ice_segments/h_li", ATL06_FILE2, asset)
        expected_32 = [350.46988, 352.08688, 352.43243, 353.19345, 353.69543, 352.25998, 350.15366, 346.37888, 342.47903, 341.51]
        bckgrd_32nf = icesat2.h5("/gt1l/bckgrd_atlas/bckgrd_rate", ATL03_FILE2, asset)
        expected_32nf = [29311.684, 6385.937, 6380.8413, 28678.951, 55349.168, 38201.082, 19083.434, 38045.67, 34942.434, 38096.266]
        for c in zip(heights_64, expected_64, heights_32, expected_32, bckgrd_32nf, expected_32nf):
            assert (round(c[0]) == round(c[1])) and (round(c[2]) == round(c[3])) and (round(c[4]) == round(c[5]))

    def test_variable_length(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        v = icesat2.h5("/gt1r/geolocation/segment_ph_cnt", ATL03_FILE1, asset)
        assert v[0] == 258 and v[1] == 256 and v[2] == 273

    def test_invalid_file(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        v = icesat2.h5("/gt1r/geolocation/segment_ph_cnt", INVALID_FILE, asset)
        assert len(v) == 0

    def test_invalid_asset(self, domain, organization):
        icesat2.init(domain, organization=organization)
        v = icesat2.h5("/gt1r/geolocation/segment_ph_cnt", ATL03_FILE1, "invalid-asset")
        assert len(v) == 0

    def test_invalid_path(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        v = icesat2.h5("/gt1r/invalid-path", ATL03_FILE1, asset)
        assert len(v) == 0
