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
@pytest.mark.network
class TestTime:
    def test_time(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
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
        assert now == again

    def test_gps2utc(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        utc = sliderule.gps2utc(1235331234000)
        assert utc == '2019-02-27T19:33:36Z'

#
# Definition API
#
@pytest.mark.network
class TestDefinition:
    def test_definition(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        rqst = {
            "rectype": "atl06rec.elevation",
        }
        d = sliderule.source("definition", rqst)
        assert d["time"]["offset"] == 192

#
# Version API
#
@pytest.mark.network
class TestVersion:
    def test_version_endpoint(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        rsps = sliderule.source("version", {})
        assert 'server' in rsps
        assert 'version' in rsps['server']
        assert 'commit' in rsps['server']
        assert 'launch' in rsps['server']
        assert 'duration' in rsps['server']
        assert 'packages' in rsps['server']
        assert '.' in rsps['server']['version']
        assert '-g' in rsps['server']['commit']
        assert ':' in rsps['server']['launch']
        assert rsps['server']['duration'] > 0
        assert 'icesat2' in rsps['server']['packages']
        assert 'version' in rsps['icesat2']
        assert 'commit' in rsps['icesat2']
        assert '.' in rsps['icesat2']['version']
        assert '-g' in rsps['icesat2']['commit']

    def test_get_version_api(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        version = sliderule.get_version()
        assert isinstance(version, dict)
        assert {'icesat2', 'server', 'client'} <= version.keys()

    def test_client_version(self):
        assert hasattr(sliderule, '__version__')
        assert isinstance(sliderule.__version__, str)

    def test_check_version(self, domain, organization, desired_nodes):
        sliderule.set_url(domain)
        sliderule.authenticate(organization)
        sliderule.scaleout(desired_nodes, 15, True)
        sliderule.check_version(plugins=['icesat2'])

#
# Initialization APIs
#
@pytest.mark.network
class TestInitialization:
    def test_loop_init(self, domain, organization, desired_nodes):
        for _ in range(10):
            icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)

    def test_loop_versions(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        for _ in range(10):
            sliderule.source("version", {})

    def test_init_badurl(self):
        with pytest.raises( (sliderule.FatalError) ):
            icesat2.init('incorrect.org:8877')
            sliderule.source("version")

    def test_set_badurl(self):
        with pytest.raises( (sliderule.FatalError) ):
            sliderule.set_rqst_timeout((1, 60))
            sliderule.set_url('incorrect.org:8877')
            sliderule.source("version")

    def test_seturl_empty(self):
        with pytest.raises(TypeError, match=('url')):
            sliderule.set_url()

    def test_init_empty_raises(self):
        with pytest.raises(TypeError):
            icesat2.init(url=[])

#
# Region of Interest APIs
#
@pytest.mark.network
class TestRegion:
    def test_toregion_empty_raises(self):
        with pytest.raises(TypeError, match=('source')):
            region = sliderule.toregion()

    def test_toregion(self):
        region = sliderule.toregion(os.path.join(TESTDIR, 'data/polygon.geojson'))
        assert len(region["poly"]) == 5 # 5 coordinate pairs
        assert {'lon', 'lat'} <= region["poly"][0].keys()
