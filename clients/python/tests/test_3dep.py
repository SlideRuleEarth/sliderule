"""Tests for sliderule 3dep raster support."""

import pytest
import numpy
import math
from sliderule import sliderule, earthdata, raster, gedi

region = [  {"lon": -108.3435200747503, "lat": 38.89102961045247},
            {"lon": -107.7677425431139, "lat": 38.90611184543033},
            {"lon": -107.7818591266989, "lat": 39.26613714985466},
            {"lon": -108.3605610678553, "lat": 39.25086131372244},
            {"lon": -108.3435200747503, "lat": 38.89102961045247}  ]

@pytest.mark.network
class Test3DEP:
    def test_sample(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region)
        gdf = raster.sample("usgs3dep-1meter-dem", [[-108.0,39.0]], {"catalog": geojson})
        assert len(gdf) >= 4

    def test_as_numpy_array(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region)
        parms = {
            "poly": region,
            "degrade_flag": 0,
            "l2_quality_flag": 1,
            "beam": 0,
            "samples": {"3dep": {"asset": "usgs3dep-1meter-dem", "catalog": geojson}}
        }
        gdf = gedi.gedi04ap(parms, resources=['GEDI04_A_2019123154305_O02202_03_T00174_02_002_02_V002.h5'], as_numpy_array=True)
        assert len(gdf) > 0
        for key in gdf.keys():
            if '3dep' in key:
                for entry in gdf[key]:
                    assert (type(entry) == numpy.ndarray) or math.isnan(entry)

    def test_as_variable(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region)
        parms = {
            "poly": region,
            "degrade_flag": 0,
            "l2_quality_flag": 1,
            "beam": 0,
            "samples": {"3dep": {"asset": "usgs3dep-1meter-dem", "catalog": geojson}}
        }
        gdf = gedi.gedi04ap(parms, resources=['GEDI04_A_2019123154305_O02202_03_T00174_02_002_02_V002.h5'], as_numpy_array=False)
        non_array_count = 0
        for key in gdf.keys():
            if '3dep' in key:
                for entry in gdf[key]:
                    if (type(entry) != numpy.ndarray) and (not math.isnan(entry)):
                        non_array_count += 1
        assert non_array_count > 0
