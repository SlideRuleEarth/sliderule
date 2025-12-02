"""Tests for sliderule icesat2 api."""

import pytest
import sliderule
from sliderule import earthdata, session
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
# AMS
#
class TestAMS:
    def test_atl13_refid(self, init):
        granules = earthdata.search({"asset": "icesat2-atl13", "atl13": {"refid": 5952002394}})
        assert init
        assert len(granules) == 43

    def test_atl13_name(self, init):
        granules = earthdata.search({"asset": "icesat2-atl13", "atl13": {"name": "Caspian Sea"}, "max_resources": 1500})
        assert init
        assert len(granules) == 1372

    def test_atl13_coord(self, init):
        granules = earthdata.search({"asset": "icesat2-atl13", "atl13": {"coord": {"lon": -77.40162711974297, "lat": 38.48769543754824}}})
        assert init
        assert len(granules) == 2

    def test_atl24_poly(self, init):
        poly = [
            {"lat": 21.222261686673306, "lon": -73.78074797284968},
            {"lat": 21.07228912392266,  "lon": -73.78074797284968},
            {"lat": 21.07228912392266,  "lon": -73.51303956051089},
            {"lat": 21.222261686673306, "lon": -73.51303956051089},
            {"lat": 21.222261686673306, "lon": -73.78074797284968}
        ]
        granules = earthdata.search({"asset": "atl24-s3", "poly": poly})
        assert init
        assert len(granules) == 89

    def test_atl24_meta(self, init):
        response = earthdata.search({"asset": "atl24-s3", "atl24": {"photons0":100}, "t0":"2019-09-30", "t1":"2019-10-02", "with_meta": True})
        assert init
        assert response["hits"] == 859
        assert len(response["granules"]) == 204

#
# CMR
#
class TestCMR:
    def test_grandmesa_time_range(self, init):
        granules = earthdata.cmr(short_name='ATL03', polygon=grandmesa, time_start='2018-10-01', time_end='2018-12-01')
        assert init
        assert isinstance(granules, list)
        assert 'ATL03_20181017222812_02950102_007_01.h5' in granules

    def test_truncation(self, domain, organization):
        sliderule.init(domain, organization=organization, rethrow=True)
        poly = [
            {"lon": -57.3377754282184,"lat": 47.52178013311578},
            {"lon": -57.3377754282184,"lat": 48.25095283140914},
            {"lon": -59.04776772612316,"lat": 48.25095283140914},
            {"lon": -59.04776772612316,"lat": 47.52178013311578},
            {"lon": -57.3377754282184,"lat": 47.52178013311578}
        ]
        with pytest.raises(session.FatalError) as info:
            earthdata.search({"asset": "icesat2", "poly": poly})
        assert "HTTP error <500>" in str(info.value)

    def test_invalid_polygon(self, domain, organization):
        sliderule.init(domain, organization=organization, rethrow=True)
        poly = [
            {"lon": 19.975596525453085, "lat": 3.7442630778936206},
            {"lon": 20.1664873571508,   "lat": 3.7620774102445154},
            {"lon": 20.425971422567812, "lat": 4.2291563179785925},
            {"lon": 20.65399992379713,  "lat": 3.5112727532072086},
            {"lon": 20.77075101805619,  "lat": 3.8854015379486952},
            {"lon": 20.132155753778108, "lat": 4.082686073723906},
            {"lon": 19.975596525453085, "lat": 3.744263077893} # not closed (missing last digits)
        ]
        with pytest.raises(session.FatalError) as info:
            earthdata.search({"asset": "icesat2", "poly": poly})
        assert "HTTP error <500>" in str(info.value)

    def test_simplification(self, init):
        poly = [
            {"lon": 19.975596525453085, "lat": 3.7442630778936206},
            {"lon": 20.1664873571508,   "lat": 3.7620774102445154},
            {"lon": 20.425971422567812, "lat": 4.2291563179785925},
            {"lon": 20.65399992379713,  "lat": 3.5112727532072086},
            {"lon": 20.77075101805619,  "lat": 3.8854015379486952},
            {"lon": 20.132155753778108, "lat": 4.082686073723906},
            {"lon": 19.975596525453085, "lat": 3.7442630778936206}
        ]
        rsps = earthdata.search({"asset": "icesat2", "poly": poly})
        assert init
        assert len(rsps) >= 160

#
# STAC
#
class TestSTAC:
    def test_hls_asdict(self, init):
        earthdata.set_max_resources(1000)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        catalog = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2021-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=False)
        assert init
        assert len(catalog["features"]) == 590
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 2

    def test_hls_asstr(self, init):
        earthdata.set_max_resources(1000)
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        response = earthdata.stac(short_name="HLS", polygon=region["poly"], time_start="2022-01-01T00:00:00Z", time_end="2022-03-01T00:00:00Z", as_str=True)
        catalog = json.loads(response)
        assert init
        assert catalog["features"][0]['properties']['eo:cloud_cover'] == 99

    def test_arcticdem_region(self, init):
        earthdata.set_max_resources(1000)
        arcticdem_test_region = [ # Central Brooks Range, northern Alaska
            { "lon": -152.0, "lat": 68.5 },
            { "lon": -152.0, "lat": 67.5 },
            { "lon": -147.0, "lat": 67.5 },
            { "lon": -147.0, "lat": 68.5 },
            { "lon": -152.0, "lat": 68.5 }
        ]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=arcticdem_test_region, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert len(catalog["features"]) == 702
        assert catalog["features"][0]['properties']['datetime'] == "2022-05-16T21:47:36Z"

    def test_arcticdem_point(self, init):
        arcticdem_test_point = [
            { "lon": -152.0, "lat": 68.5 }
        ]
        catalog = earthdata.stac(short_name="arcticdem-strips", polygon=arcticdem_test_point, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert init
        assert len(catalog["features"]) == 16
        assert catalog["features"][0]['properties']['datetime'] == "2022-03-24T22:38:46Z"

    def test_rema_region(self, init):
        earthdata.set_max_resources(1000)
        rema_test_region = [ # Ellsworth Mountains, West Antarctica
            { "lon": -86.0, "lat": -78.0 },
            { "lon": -86.0, "lat": -79.0 },
            { "lon": -82.0, "lat": -79.0 },
            { "lon": -82.0, "lat": -78.0 },
            { "lon": -86.0, "lat": -78.0 }
        ]
        catalog = earthdata.stac(short_name="rema-strips", polygon=rema_test_region, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert init
        assert len(catalog["features"]) == 603
        assert catalog["features"][0]['properties']['datetime'] == "2024-12-21T14:07:17Z"

    def test_rema_point(self, init):
        rema_test_point = [
            { "lon": -86.0, "lat": -78.0 }
        ]
        catalog = earthdata.stac(short_name="rema-strips", polygon=rema_test_point, time_start="2000-01-01T00:00:00Z", as_str=False)
        assert init
        assert len(catalog["features"]) == 49
        assert catalog["features"][0]['properties']['datetime'] == "2024-11-07T14:25:48Z"

    def test_bad_short_name(self, init):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        assert init
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
# Icesat2 Earthdata Search
#
class TestIcesat2:
    def test_atl03(self, init):
        parms = {"asset": "icesat2", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert len(resources) == 26
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert resource in resources
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime('2019-12-01', '%Y-%m-%d')

    def test_atl06(self, init):
        parms = {"asset": "icesat2-atl06", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert len(resources) == 26
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime('2019-12-01', '%Y-%m-%d')

    def test_atl06_default_version(self, init):
        parms = {"asset": "icesat2-atl06", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert resources
        assert all("_007_" in resource for resource in resources)

    def test_atl06_version_006(self, init):
        parms = {"asset": "icesat2-atl06", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01', "cmr": {"version": "006"}}
        resources = earthdata.search(parms)
        assert init
        assert resources
        assert all("_006_" in resource for resource in resources)

    def test_atl06_version_007(self, init):
        parms = {"asset": "icesat2-atl06", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01', "cmr": {"version": "007"}}
        resources = earthdata.search(parms)
        assert init
        assert resources
        assert all("_007_" in resource for resource in resources)

    def test_atl09(self, init):
        parms = {"asset": "icesat2-atl09", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert len(resources) == 36
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime('2019-12-01', '%Y-%m-%d')

    def test_atl13_cmr(self, init):
        saltlake = [
            {"lon": -111.78656302303546,"lat": 40.474445992545355},
            {"lon": -111.78656302303546,"lat": 41.745511885629725},
            {"lon": -113.25842634761666,"lat": 41.745511885629725},
            {"lon": -113.25842634761666,"lat": 40.474445992545355},
            {"lon": -111.78656302303546,"lat": 40.474445992545355}
        ]
        resources = earthdata.cmr(short_name="ATL13", polygon=saltlake, time_start='2018-10-01', time_end='2019-12-01')
        assert init
        assert len(resources) == 67
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert datetime.strptime(resource[6:14], '%Y%m%d') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[6:14], '%Y%m%d') <= datetime.strptime('2019-12-01', '%Y-%m-%d')

#
# GEDI Earthdata Search
#
class TestGedi:
    def test_l1b(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "asset": "gedil1b",
            "poly": region["poly"],
        }
        granules = earthdata.search(parms)
        assert 'GEDI01_B_2019109210809_O01988_03_T02056_02_005_01_V002.h5' in granules

    def test_l1b_r2(self, init):
        parms = {"asset": "gedil1b", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert len(resources) == 26
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert datetime.strptime(resource[9:16], '%Y%j') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[9:16], '%Y%j') <= datetime.strptime('2019-12-01', '%Y-%m-%d')

    def test_l2a(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "asset": "gedil2a",
            "poly": region["poly"],
        }
        granules = earthdata.search(parms)
        assert 'GEDI02_A_2019109210809_O01988_03_T02056_02_003_01_V002.h5' in granules

    def test_l2a_r2(self, init):
        parms = {"asset": "gedil2a", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert len(resources) == 26
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert datetime.strptime(resource[9:16], '%Y%j') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[9:16], '%Y%j') <= datetime.strptime('2019-12-01', '%Y-%m-%d')

    def test_l4a(self):
        region = sliderule.toregion(os.path.join(TESTDIR, "data", "grandmesa.geojson"))
        parms = {
            "asset": "gedil4a",
            "poly": region["poly"],
        }
        granules = earthdata.search(parms)
        assert 'GEDI04_A_2019109210809_O01988_03_T02056_02_002_02_V002.h5' in granules

    def test_l4a_r2(self, init):
        parms = {"asset": "gedil4a", "poly": grandmesa, "t0": '2018-10-01', "t1": '2019-12-01'}
        resources = earthdata.search(parms)
        assert init
        assert len(resources) == 26
        assert len(resources) == len(set(resources))
        for resource in resources:
            assert datetime.strptime(resource[9:16], '%Y%j') >= datetime.strptime('2018-10-01', '%Y-%m-%d')
            assert datetime.strptime(resource[9:16], '%Y%j') <= datetime.strptime('2019-12-01', '%Y-%m-%d')
