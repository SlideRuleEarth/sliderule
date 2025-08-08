"""Tests for sliderule APIs."""

import pytest
import sliderule
from sliderule import icesat2
from pathlib import Path
import os

# Set test directory
TESTDIR = Path(__file__).parent

# Change connection timeout from default 10s to 1s
sliderule.set_rqst_timeout((1, 60))

#
# Time API
#
class TestTime:
    def test_time(self, init):
        rqst = {
            "time": "NOW",
            "input": "NOW",
            "output": "GPS" }
        d = sliderule.source("time", rqst)
        now = d["time"] - (d["time"] % 1000) # gmt is in resolution of seconds, not milliseconds
        rqst["time"] = d["time"]
        rqst["input"] = "GPS"
        rqst["output"] = "GMT"
        d = sliderule.source("time", rqst)
        rqst["time"] = d["time"]
        rqst["input"] = "GMT"
        rqst["output"] = "GPS"
        d = sliderule.source("time", rqst)
        again = d["time"]
        assert init
        assert now == again

    def test_gps2utc(self, init):
        utc = sliderule.gps2utc(1235331234)
        assert init
        assert utc == '2019-02-27T19:33:36Z'

#
# Definition API
#
class TestDefinition:
    def test_definition(self, init):
        rqst = {
            "rectype": "atl06rec.elevation",
        }
        d = sliderule.source("definition", rqst)
        assert init
        assert d["time"]["offset"] == 192

#
# Version API
#
class TestVersion:
    def test_version_endpoint(self, init):
        rsps = sliderule.source("version", {})
        assert init
        assert 'server' in rsps
        assert 'version' in rsps['server']
        assert 'build' in rsps['server']
        assert 'launch' in rsps['server']
        assert 'duration' in rsps['server']
        assert 'packages' in rsps['server']
        assert '.' in rsps['server']['version']
        assert len(rsps['server']['build']) > 0
        assert ':' in rsps['server']['launch']
        assert rsps['server']['duration'] > 0

    def test_get_version_api(self, init):
        version = sliderule.get_version()
        assert init
        assert isinstance(version, dict)
        assert {'server', 'client'} <= version.keys()

    def test_client_version(self):
        assert hasattr(sliderule, '__version__')
        assert isinstance(sliderule.__version__, str)

    def test_check_version(self, domain, organization, desired_nodes):
        sliderule.set_url(domain)
        sliderule.authenticate(organization)
        sliderule.scaleout(desired_nodes, 15, True)
        assert sliderule.check_version()

#
# Initialization APIs
#
class TestInitialization:
    def test_loop_init(self, domain, organization, desired_nodes):
        for _ in range(10):
            icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)

    def test_loop_versions(self, init):
        assert init
        for _ in range(10):
            rsps = sliderule.source("version", {})
            assert len(rsps) > 0

    def test_init_badurl(self):
        with pytest.raises( (sliderule.session.FatalError) ):
            icesat2.init('incorrect.org:8877', rethrow=True)

    def test_set_badurl(self):
        sliderule.init() # resets session state
        sliderule.set_rqst_timeout((1, 60))
        sliderule.set_url('incorrect.org:8877')
        rsps = sliderule.source("version")
        assert rsps == None

    def test_seturl_empty(self):
        with pytest.raises(TypeError, match=('url')):
            sliderule.set_url()

    def test_init_empty_raises(self):
        with pytest.raises(TypeError):
            icesat2.init(url=[])

    def test_init_restore(self):
            sliderule.init()
            assert len(sliderule.source("version")) > 0

#
# Region of Interest APIs
#
class TestRegion:
    def test_toregion_empty_raises(self):
        with pytest.raises(TypeError, match=('source')):
            region = sliderule.toregion()

    def test_toregion(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        assert len(region["poly"]) == 5 # 5 coordinate pairs
        assert {'lon', 'lat'} <= region["poly"][0].keys()
