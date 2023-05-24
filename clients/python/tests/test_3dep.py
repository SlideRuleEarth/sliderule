"""Tests for sliderule l3dep raster support."""

import pytest
from pathlib import Path
import os.path
from sliderule import sliderule, earthdata, raster

TESTDIR = Path(__file__).parent

@pytest.mark.network
class Test3DEP:
    def test_sample(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                   {"lon": -107.7677425431139, "lat": 38.90611184543033},
                   {"lon": -107.7818591266989, "lat": 39.26613714985466},
                   {"lon": -108.3605610678553, "lat": 39.25086131372244},
                   {"lon": -108.3435200747503, "lat": 38.89102961045247} ]
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region)
        gdf = raster.sample("usgs3dep-1meter-dem", [[-108.0,39.0]], {"catalog": geojson})
        assert(len(gdf) >= 4)