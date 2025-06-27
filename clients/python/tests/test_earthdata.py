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

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

# Define regions of interest
grandmesa = [ {"lon": -108.3435200747503, "lat": 38.89102961045247},
              {"lon": -107.7677425431139, "lat": 38.90611184543033},
              {"lon": -107.7818591266989, "lat": 39.26613714985466},
              {"lon": -108.3605610678553, "lat": 39.25086131372244},
              {"lon": -108.3435200747503, "lat": 38.89102961045247} ]

#
# CMR
#
@pytest.mark.external
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
@pytest.mark.external
class TestSTAC:
    def test_hls_asdict(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        catalog = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2021-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=False)
        assert len(catalog["features"]) == 590
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 2

    def test_hls_asstr(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        response = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=True)
        catalog = json.loads(response)
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 99

    def test_arcticdem_region(self):
        arcticdem_test_region = [[ # Central Brooks Range, northern Alaska
            { "lon": -152.0, "lat": 68.5 },
            { "lon": -152.0, "lat": 67.5 },
            { "lon": -147.0, "lat": 67.5 },
            { "lon": -147.0, "lat": 68.5 },
            { "lon": -152.0, "lat": 68.5 }
        ]]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=arcticdem_test_region, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert len(catalog["features"]) == 702
        assert catalog["features"][0]['properties']['datetime'] == "2022-05-16T21:47:36Z"

    def test_arcticdem_point(self):
        arcticdem_test_point = [[
            { "lon": -152.0, "lat": 68.5 }
        ]]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=arcticdem_test_point, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert len(catalog["features"]) == 16
        assert catalog["features"][0]['properties']['datetime'] == "2022-03-24T22:38:46Z"

    def test_rema_region(self):
        rema_test_region = [[ # Ellsworth Mountains, West Antarctica
            { "lon": -86.0, "lat": -78.0 },
            { "lon": -86.0, "lat": -79.0 },
            { "lon": -82.0, "lat": -79.0 },
            { "lon": -82.0, "lat": -78.0 },
            { "lon": -86.0, "lat": -78.0 }
        ]]
        catalog = earthdata.stac(short_name="rema-strips", polygon=rema_test_region, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert len(catalog["features"]) == 603
        assert catalog["features"][0]['properties']['datetime'] == "2024-12-21T14:07:17Z"

    def test_rema_point(self):
        rema_test_point = [[
            { "lon": -86.0, "lat": -78.0 }
        ]]
        catalog = earthdata.stac(short_name="rema-strips", polygon=rema_test_point, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert len(catalog["features"]) == 49
        assert catalog["features"][0]['properties']['datetime'] == "2024-11-07T14:25:48Z"

    def test_bad_short_name(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        with pytest.raises(sliderule.FatalError):
            earthdata.stac(short_name="DOES_NOT_EXIST", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z")
#
# TNM
#
@pytest.mark.external
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
@pytest.mark.external
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

#
# GEDI Earthdata Search
#
@pytest.mark.external
class TestGedi:
    def test_l1b(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "asset": "gedil1b",
            "poly": region["poly"],
        }
        granules = earthdata.search(parms)
        assert 'GEDI01_B_2019109210809_O01988_03_T02056_02_005_01_V002.h5' in granules

    def test_l2a(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "asset": "gedil2a",
            "poly": region["poly"],
        }
        granules = earthdata.search(parms)
        assert 'GEDI02_A_2019109210809_O01988_03_T02056_02_003_01_V002.h5' in granules

    def test_l4a(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "asset": "gedil4a",
            "poly": region["poly"],
        }
        granules = earthdata.search(parms)
        assert 'GEDI04_A_2019109210809_O01988_03_T02056_02_002_02_V002.h5' in granules
