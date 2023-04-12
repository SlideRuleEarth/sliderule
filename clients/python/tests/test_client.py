"""Tests for sliderule-python."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule

class TestLocal:
    def test_version(self):
        assert hasattr(sliderule, '__version__')
        assert isinstance(sliderule.__version__, str)

    def test_seturl_empty(self):
        with pytest.raises(TypeError, match=('url')):
            sliderule.set_url()

    def test_gps2utc(self, domain, organization, desired_nodes):
        sliderule.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        utc = sliderule.gps2utc(1235331234000)
        assert utc == '2019-02-27T19:33:36Z'

@pytest.mark.network
class TestRemote:
    def test_check_version(self, domain, organization, desired_nodes):
        sliderule.set_url(domain)
        sliderule.authenticate(organization)
        sliderule.scaleout(desired_nodes, 15, True)
        sliderule.check_version(plugins=['icesat2'])

    def test_init_badurl(self):
        with pytest.raises( (sliderule.FatalError) ):
            sliderule.set_rqst_timeout((1, 60))
            sliderule.set_url('incorrect.org:8877')
            sliderule.source("version")
