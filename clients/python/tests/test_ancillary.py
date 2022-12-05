"""Tests for sliderule-python icesat2 api."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule
from sliderule import icesat2
from pathlib import Path
import os.path

TESTDIR = Path(__file__).parent

@pytest.mark.network
class TestRemote:

    def test_geo(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        region = icesat2.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "srt":              icesat2.SRT_LAND,
            "atl03_geo_fields": ["solar_elevation"]
        }
        gdf = icesat2.atl06p(parms, asset, resources=["ATL03_20181017222812_02950102_005_01.h5"])
        assert len(gdf["solar_elevation"]) == 1180
        assert gdf['solar_elevation'].describe()["min"] - 20.803468704223633 < 0.0000001

    def test_ph(self, domain, asset, organization):
        icesat2.init(domain, organization=organization)
        region = icesat2.toregion(os.path.join(TESTDIR, "data/grandmesa.geojson"))
        parms = {
            "poly":             region["poly"],
            "srt":              icesat2.SRT_LAND,
            "atl03_ph_fields":  ["ph_id_count"]
        }
        gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_005_01.h5", asset)
        assert gdf["ph_id_count"][0] == 2
        assert gdf["ph_id_count"][1] == 1
        assert gdf["ph_id_count"][2] == 2
        assert gdf["ph_id_count"][3] == 1
        assert gdf["ph_id_count"][4] == 1
        assert len(gdf["ph_id_count"]) == 410233
