import pytest
from sliderule import sliderule

class TestAtl13:
    def test_refid(self, init):
        response = sliderule.source("ams", {"refid": 5952002394})
        assert init
        assert len(response["granules"]) == 43

    def test_name(self, init):
        response = sliderule.source("ams", {"name": "Caspian Sea"})
        assert init
        assert len(response["granules"]) == 1372

    def test_coord(self, init):
        response = sliderule.source("ams", {"coord": {"lon": -77.40162711974297, "lat": 38.48769543754824}})
        assert init
        assert len(response["granules"]) == 2
