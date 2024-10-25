"""Tests for sliderule icesat2 api."""

import pytest
import sliderule
from sliderule import earthdata
from sliderule.earthdata import __cmr_collection_query as cmr_collection_query
from sliderule.earthdata import __cmr_max_version as cmr_max_version
from pathlib import Path
from datetime import datetime
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
    def test_grandmesa_time_range(self, init):
        granules = earthdata.cmr(short_name='ATL03', polygon=grandmesa, time_start='2018-10-01', time_end='2018-12-01')
        assert init
        assert isinstance(granules, list)
        assert 'ATL03_20181017222812_02950102_006_02.h5' in granules

    def test_collection(self, init):
        entries = cmr_collection_query('NSIDC_ECS', 'ATL03')
        assert init
        assert isinstance(entries, list)
        assert entries[0]['short_name'] == 'ATL03'

    def test_max_version(self, init):
        max_version = cmr_max_version('NSIDC_ECS', 'ATL03')
        assert init
        assert isinstance(max_version, str)
        assert int(max_version) >= 6

#
# STAC
#
@pytest.mark.network
class TestSTAC:
    def test_asdict(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        catalog = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=False)
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 99

    def test_asstr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        response = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=True)
        catalog = json.loads(response)
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 99

    def test_bad_short_name(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        with pytest.raises(sliderule.FatalError):
            earthdata.stac(short_name="DOES_NOT_EXIST", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z")

#
# TNM
#
@pytest.mark.network
class TestTNM:
    def test_asdict(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        geojson = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region["poly"], as_str=False)
        assert 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m' in geojson["features"][0]['properties']['url']

    def test_asstr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        response = earthdata.tnm(short_name='Digital Elevation Model (DEM) 1 meter', polygon=region["poly"], as_str=True)
        geojson = json.loads(response)
        assert 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m' in geojson["features"][0]['properties']['url']

    def test_bad_short_name(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        geojson = earthdata.tnm(short_name='DOES_NOT_EXIST', polygon=region["poly"], as_str=False)
        assert len(geojson['features']) == 0

#
# SlideRule Endpoint
#
@pytest.mark.network
class TestSlideRule:
    def test_atl03(self, init):
        parms = {"asset": "icesat2", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        assert init
        assert len(rsps) == 26
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime(parms["t1"], '%Y-%m-%d')

    def test_atl06(self, init):
        parms = {"asset": "icesat2-atl06", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        assert init
        assert len(rsps) == 26
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime(parms["t1"], '%Y-%m-%d')

    def test_atl09(self, init):
        parms = {"asset": "icesat2-atl09", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        assert init
        assert len(rsps) == 36
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime(parms["t1"], '%Y-%m-%d')

    def test_atl13(self, init):
        saltlake = [
            {
            "lon": -111.78656302303546,
            "lat": 40.474445992545355
            },
            {
            "lon": -111.78656302303546,
            "lat": 41.745511885629725
            },
            {
            "lon": -113.25842634761666,
            "lat": 41.745511885629725
            },
            {
            "lon": -113.25842634761666,
            "lat": 40.474445992545355
            },
            {
            "lon": -111.78656302303546,
            "lat": 40.474445992545355
            }
        ]
        parms = {"asset": "icesat2-atl13", "poly": saltlake, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        assert init
        print(resources)
        assert len(rsps) == 67
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime(parms["t1"], '%Y-%m-%d')

    def test_gedil1b(self, init):
        parms = {"asset": "gedil1b", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        print(resources)
        assert init
        assert len(rsps) == 26
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[9:16], '%Y%j') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[9:16], '%Y%j') <= datetime.strptime(parms["t1"], '%Y-%m-%d')

    def test_gedil2a(self, init):
        parms = {"asset": "gedil2a", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        print(resources)
        assert init
        assert len(rsps) == 26
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[9:16], '%Y%j') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[9:16], '%Y%j') <= datetime.strptime(parms["t1"], '%Y-%m-%d')

    def test_gedil4a(self, init):
        parms = {"asset": "gedil4a", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        rsps = sliderule.source("earthdata", parms)
        print(resources)
        assert init
        assert len(rsps) == 26
        assert len(rsps) == len(set(rsps))
        assert len(rsps) == len(resources)
        for resource in resources:
            assert resource in rsps
            assert datetime.strptime(resource[9:16], '%Y%j') >= datetime.strptime(parms["t0"], '%Y-%m-%d')
            assert datetime.strptime(resource[9:16], '%Y%j') <= datetime.strptime(parms["t1"], '%Y-%m-%d')
