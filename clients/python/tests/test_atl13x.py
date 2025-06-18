import pytest
from sliderule import sliderule

class TestAtl13x:
    def test_refid(self, init):
        parms = { "atl13": { "refid": 5952002394 } }
        gdf = sliderule.run("atl13x", parms)
        assert init
        assert len(gdf) >= 5032

    def test_name(self, init):
        region = sliderule.toregion({
            "type": "FeatureCollection",
            "features": [ { "type": "Feature", "properties": {}, "geometry": { "coordinates": [ [ [49.63305859097062, 43.517023064094445], [49.63305859097062, 43.26673730943335], [50.39096571145933, 43.26673730943335], [50.39096571145933, 43.517023064094445], [49.63305859097062, 43.517023064094445] ] ], "type": "Polygon" } } ]
        })
        parms = { "atl13": { "name": "Caspian Sea" }, "poly": region["poly"], "max_resources": 500, "t0": '2022-01-01', "t1": '2023-01-01'}
        gdf = sliderule.run("atl13x", parms)
        assert init
        assert len(gdf) >= 3275

    def test_coord(self, init):
        parms = { "atl13": {"coord": {"lon": -77.40162711974297, "lat": 38.48769543754824}} }
        gdf = sliderule.run("atl13x", parms)
        assert init
        assert len(gdf) >= 138
