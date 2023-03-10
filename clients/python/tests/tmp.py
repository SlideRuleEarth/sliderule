import sliderule
from sliderule import icesat2

icesat2.init("localhost", verbose=True, organization=None)
region = sliderule.toregion("tests/data/grandmesa.geojson")
parms = {
    "poly": region["poly"],
    "srt": icesat2.SRT_LAND,
    "cnf": icesat2.CNF_SURFACE_HIGH,
    "ats": 10.0,
    "cnt": 10,
    "len": 40.0,
    "res": 20.0,
}
gdf = icesat2.atl03s(parms, "ATL03_20181017222812_02950102_005_01.h5", "nsidc-s3")
print(gdf)