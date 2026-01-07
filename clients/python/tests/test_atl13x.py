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
        poly_points = [
            { "lon": -96.88981325008096, "lat": 40.837055699310184 },
            { "lon": -96.86808274338642, "lat": 40.82851431769919 },
            { "lon": -96.85261897514089, "lat": 40.84322718702893 },
            { "lon": -96.86342385851648, "lat": 40.86183431906426 },
            { "lon": -96.89796118392523, "lat": 40.86494776390094 },
            { "lon": -96.88981325008096, "lat": 40.837055699310184 }
        ]
        gdf_coord = sliderule.run("atl13x",
            {
                "atl13": {
                    "coord": {
                        "lon": -96.87354790276703,
                        "lat": 40.846535052574865
                    }
                },
            }, resources=["ATL13_20240707040137_03012401_006_01.h5"])
        # Pawnee Lake Polygon
        gdf_poly = sliderule.run("atl13x",
            {
                "poly": poly_points
            }, resources=["ATL13_20240707040137_03012401_006_01.h5"])
        # Checks
        assert init
        assert len(gdf_coord) == 111
        assert len(gdf_coord) == len(gdf_poly)

        # Bbox sanity: all polygon results within drawn bounds
        poly_lats = [p["lat"] for p in poly_points]
        poly_lons = [p["lon"] for p in poly_points]
        assert "geometry" in gdf_poly, f"missing geometry column: {list(gdf_poly.columns)}"
        lat_series = gdf_poly.geometry.y
        lon_series = gdf_poly.geometry.x
        assert lat_series.between(min(poly_lats), max(poly_lats)).all()
        assert lon_series.between(min(poly_lons), max(poly_lons)).all()
        # Column length consistency
        for col in ["time_ns", "latitude", "longitude", "ht_ortho", "ht_water_surf", "water_depth"]:
            if col in gdf_poly:
                assert len(gdf_poly[col]) == len(gdf_poly)

    def test_refid_poly_interaction(self, init):
        # Ref ID only
        gdf_refid = sliderule.run("atl13x", {"atl13": {"refid": 5952002394}}, resources=["ATL13_20240707040137_03012401_006_01.h5"])

        # Same ref ID plus polygon
        poly_points = [
            { "lon": -96.88981325008096, "lat": 40.837055699310184 },
            { "lon": -96.86808274338642, "lat": 40.82851431769919 },
            { "lon": -96.85261897514089, "lat": 40.84322718702893 },
            { "lon": -96.86342385851648, "lat": 40.86183431906426 },
            { "lon": -96.89796118392523, "lat": 40.86494776390094 },
            { "lon": -96.88981325008096, "lat": 40.837055699310184 }
        ]
        gdf_refid_poly = sliderule.run("atl13x", {"atl13": {"refid": 5952002394}, "poly": poly_points}, resources=["ATL13_20240707040137_03012401_006_01.h5"])

        assert init
        assert len(gdf_refid_poly) <= len(gdf_refid)
        poly_lats = [p["lat"] for p in poly_points]
        poly_lons = [p["lon"] for p in poly_points]
        assert "geometry" in gdf_refid_poly, f"missing geometry column: {list(gdf_refid_poly.columns)}"
        lat_series = gdf_refid_poly.geometry.y
        lon_series = gdf_refid_poly.geometry.x
        assert lat_series.between(min(poly_lats), max(poly_lats)).all()
        assert lon_series.between(min(poly_lons), max(poly_lons)).all()

    def test_empty_poly(self, init):
        # Polygon far from the granule footprint
        empty_poly = [
            { "lon": 0.0, "lat": 0.0 },
            { "lon": 0.1, "lat": 0.0 },
            { "lon": 0.1, "lat": 0.1 },
            { "lon": 0.0, "lat": 0.1 },
            { "lon": 0.0, "lat": 0.0 },
        ]
        gdf_empty = sliderule.run("atl13x", {"poly": empty_poly}, resources=["ATL13_20240707040137_03012401_006_01.h5"])
        assert init
        assert len(gdf_empty) == 0
