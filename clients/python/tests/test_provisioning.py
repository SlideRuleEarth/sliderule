"""Tests for sliderule-python icesat2 api."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule
from sliderule import icesat2

@pytest.mark.network
class TestProvisioning:
    def test_authenticate(self, server, organization):
        sliderule.set_url(server)
        status = sliderule.authenticate(organization)
        assert status

    def test_bad_org(self, server):
        sliderule.set_url(server)
        status = sliderule.authenticate("non_existent_org")
        assert status == False

    def test_bad_creds(self, server, organization):
        sliderule.set_url(server)
        status = sliderule.authenticate(organization, "missing_user", "wrong_password")
        assert status == False
