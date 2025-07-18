"""Tests for sliderule 3dep raster support."""

import pytest
import numpy
import math
from sliderule import earthdata, raster, gedi, icesat2

region = [  {"lon": -108.3435200747503, "lat": 38.89102961045247},
            {"lon": -107.7677425431139, "lat": 38.90611184543033},
            {"lon": -107.7818591266989, "lat": 39.26613714985466},
            {"lon": -108.3605610678553, "lat": 39.25086131372244},
            {"lon": -108.3435200747503, "lat": 38.89102961045247}  ]

sigma = 1.0e-9

@pytest.mark.external
class Test3DEP_1meter:
    def test_sample(self, init):
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region)
        gdf = raster.sample("usgs3dep-1meter-dem", [[-108.0,39.0]], {"catalog": geojson})
        assert init
        assert len(gdf) >= 4

    def test_as_numpy_array(self, init):
        parms = {
            "poly": region,
            "degrade_filter": True,
            "l2_quality_filter": True,
            "beams": 0,
            "samples": {"3dep": {"asset": "usgs3dep-1meter-dem"}}
        }
        gdf = gedi.gedi04ap(parms, resources=['GEDI04_A_2019123154305_O02202_03_T00174_02_002_02_V002.h5'], as_numpy_array=True)
        assert init
        assert len(gdf) > 0
        for key in gdf.keys():
            if '3dep' in key:
                for entry in gdf[key]:
                    assert (type(entry) == numpy.ndarray) or math.isnan(entry)

    def test_as_variable(self, init):
        parms = {
            "poly": region,
            "degrade_filter": True,
            "l2_quality_filter": True,
            "beams": 0,
            "samples": {"3dep": {"asset": "usgs3dep-1meter-dem"}}
        }
        gdf = gedi.gedi04ap(parms, resources=['GEDI04_A_2019123154305_O02202_03_T00174_02_002_02_V002.h5'], as_numpy_array=False)
        assert init
        non_array_count = 0
        for key in gdf.keys():
            if '3dep' in key:
                for entry in gdf[key]:
                    if (type(entry) != numpy.ndarray) and (not math.isnan(entry)):
                        non_array_count += 1
        assert non_array_count > 0

class Test3DEP_10meter:
    def test_sample(self, init):
        gdf = raster.sample("usgs3dep-10meter-dem", [[-108.0,39.0]], {"slope_aspect": True, "slope_scale_length": 40})
        assert init
        assert len(gdf) == 1
        assert gdf["slope_count"].iat[0] == 25    # number of valid pixels used for spatial derivative calculation
        assert abs(gdf["slope"].iat[0]  - 6.4923495863514) < sigma
        assert abs(gdf["aspect"].iat[0] - 334.33081719763) < sigma

    def test_batch_sample(self, init):
        # Create 10,000 points in the bounding box (-108.2, 38.9) to (-107.8, 39.1)
        lons = numpy.linspace(-108.2, -107.8, 100)
        lats = numpy.linspace(38.9, 39.1, 100)
        points = [[lon, lat] for lat in lats for lon in lons]  # 100 x 100 = 10,000 points
        gdf = raster.sample("usgs3dep-10meter-dem", points)
        assert init
        assert len(gdf) == 10000

    def test_legacy_sample_with_slope_aspect(self, init):
        small_region = [
            {"lon": -108.05, "lat": 39.00},
            {"lon": -108.00, "lat": 39.00},
            {"lon": -108.00, "lat": 39.05},
            {"lon": -108.05, "lat": 39.05},
            {"lon": -108.05, "lat": 39.00}
        ]

        parms = {
            "poly": small_region,
            "srt": icesat2.SRT_LAND,
            "cnf": icesat2.CNF_SURFACE_HIGH,
            "atl08_class": "atl08_ground",
            "ats": 10.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0,
            "maxi": 10,
            "atl06_fields": ["ground_track/ref_azimuth"],
            "atl08_fields": ["segment_snowcover", "segment_landcover", "beam_azimuth", "segment_watermask", "urban_flag"],
            "samples": {
                "3dep": {"asset": "usgs3dep-10meter-dem", "slope_aspect":True}
                        },
            't0': '2020-10-13T00:00:00Z',
            't1': '2021-10-13T00:00:00Z'
        }

        gdf = icesat2.atl06p(parms)
        assert init
        assert len(gdf) == 1073
        assert gdf["3dep.slope"].describe()["count"] == 1073
        assert gdf["3dep.aspect"].describe()["count"] == 1073
        assert gdf["3dep.count"].describe()["count"] == 1073

        assert gdf["3dep.count"].iloc[0] == 9
        assert abs(gdf["3dep.slope"].iloc[0]  - 5.615220456398121) < sigma
        assert abs(gdf["3dep.aspect"].iloc[0] - 68.08075892919926) < sigma
        # print(gdf[["3dep.slope", "3dep.aspect", "3dep.count"]])

