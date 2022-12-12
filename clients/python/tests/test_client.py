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

    def test_gps2utc(self):
        utc = sliderule.gps2utc(1235331234)
        assert utc == '2019-02-27 19:34:03'

@pytest.mark.network
class TestRemote:
    def test_check_version(self, domain, organization):
        sliderule.set_url(domain)
        sliderule.authenticate(organization)
        sliderule.check_version(plugins=['icesat2'])

    def test_init_badurl(self):
        with pytest.raises( (sliderule.FatalError) ):
            sliderule.set_rqst_timeout((1, 60))
            sliderule.set_url('incorrect.org:8877')
            sliderule.source("version")
