from sliderule import sliderule, icesat2

sliderule.init("localhost", organization=None, verbose=True)

resource = "ATL03_20190314093716_11600203_006_02.h5"

region = sliderule.toregion("tests/data/dicksonfjord.geojson")

parms = { "poly": region['poly'],
          "cnf": "atl03_high",
          "ats": 20.0,
          "cnt": 10,
          "len": 40.0,
          "res": 20.0,
          "pass_invalid": True,
          "output": { "path": "testfile.csv", "format": "csv", "open_on_complete": True, "as_geo": False } }

gdf = icesat2.atl24gp(parms, resources=[resource], keep_id=True)
