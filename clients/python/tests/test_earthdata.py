"""Tests for sliderule icesat2 api."""

import pytest
import sliderule
from sliderule import icesat2, earthdata
from pathlib import Path
import os
import json

# Set test directory
TESTDIR = Path(__file__).parent

# Define region of interest
grandmesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
                {"lon": -107.7677425431139, "lat": 38.90611184543033},
                {"lon": -107.7818591266989, "lat": 39.26613714985466},
                {"lon": -108.3605610678553, "lat": 39.25086131372244},
                {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

#
# CMR
#
@pytest.mark.network
class TestCMR:
    def test_grandmesa_time_range(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        granules = earthdata.cmr(short_name='ATL03', polygon=grandmesa, time_start='2018-10-01', time_end='2018-12-01')
        assert isinstance(granules, list)
        assert 'ATL03_20181017222812_02950102_005_01.h5' in granules

#
# STAC
#
@pytest.mark.network
class TestSTAC:
    def test_asdict(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        catalog = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=False)
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 99

    def test_asstr(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        response = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=True)
        catalog = json.loads(response)
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 99

    def test_bad_short_name(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        with pytest.raises(sliderule.FatalError):
            earthdata.stac(short_name="DOES_NOT_EXIST", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z")

#
# TNM
#
@pytest.mark.network
class TestTNM:
    def test_asdict(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region["poly"], as_str=False)
        assert 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m' in geojson["features"][0]['properties']['url']

    def test_asstr(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        response = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region["poly"], as_str=True)
        geojson = json.loads(response)
        assert 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m' in geojson["features"][0]['properties']['url']

    def test_bad_short_name(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        geojson = earthdata.tnm(short_name='DOES_NOT_EXIST', polygon=region["poly"], as_str=False)
        assert len(geojson['features']) == 0