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
        assert len(gdf) == 520

    def test_coord(self, init):
        parms = { "atl13": {"coord": {"lon": -77.40162711974297, "lat": 38.48769543754824}} }
        gdf = sliderule.run("atl13x", parms)
        assert init
        assert len(gdf) >= 138

    def test_poly(self, init):
        # Pawnee Lake Coordinates
        gdf_coord = sliderule.run("atl13x",
            {
                "atl13": {
                    "refid": 0,
                    "name": "",
                    "coord": {
                        "lon": -96.87354790276703,
                        "lat": 40.846535052574865
                    }
                },
            }, resources=["ATL13_20240707040137_03012401_006_01.h5"])
        # Pawnee Lake Polygon
        gdf_poly = sliderule.run("atl13x",
            {
                "poly": [
                    { "lon": -96.88981325008096, "lat": 40.837055699310184 },
                    { "lon": -96.86808274338642, "lat": 40.82851431769919 },
                    { "lon": -96.85261897514089, "lat": 40.84322718702893 },
                    { "lon": -96.86342385851648, "lat": 40.86183431906426 },
                    { "lon": -96.89796118392523, "lat": 40.86494776390094 },
                    { "lon": -96.88981325008096, "lat": 40.837055699310184 }
                ]
            }, resources=["ATL13_20240707040137_03012401_006_01.h5"])
        # Checks
        assert init
        assert len(gdf_coord) == 111
        assert len(gdf_coord) == len(gdf_poly)
