"""Tests for sliderule gedi plugin."""

import pytest
from sliderule import sliderule, earthdata
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

@pytest.mark.external
class Test_ECCO_LLC4320:
    def test_cmr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        granules = earthdata.cmr(short_name="SWOT_SIMULATED_L2_KARIN_SSH_ECCO_LLC4320_CALVAL_V1", polygon=region["poly"], time_start=None)
        assert len(granules) > 0
        assert 'SWOT_L2_' in granules[0]

@pytest.mark.external
class Test_GLORYS:
    def test_cmr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        granules = earthdata.cmr(short_name="SWOT_SIMULATED_L2_KARIN_SSH_GLORYS_CALVAL_V1", polygon=region["poly"], time_start=None)
        assert len(granules) > 0
        assert 'SWOT_L2_' in granules[0]

