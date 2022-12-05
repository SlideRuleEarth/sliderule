"""Tests for sliderule-python icesat2 api."""

import pytest
from requests.exceptions import ConnectTimeout, ConnectionError
import sliderule
from sliderule import icesat2

@pytest.mark.network
class TestProvisioning:
    def test_authenticate(self, domain, organization):
        sliderule.set_url(domain)
        status = sliderule.authenticate(organization)
        assert status

    def test_num_nodes_update(self, domain, organization):
        sliderule.set_url(domain)
        status = sliderule.authenticate(organization)
        assert status
        result = sliderule.update_available_servers(7,20)
        assert len(result) == 2
        assert type(result[0]) == int
        assert type(result[1]) == int

    def test_bad_org(self, domain):
        sliderule.set_url(domain)
        status = sliderule.authenticate("non_existent_org")
        assert status == False

    def test_bad_creds(self, domain, organization):
        sliderule.set_url(domain)
        status = sliderule.authenticate(organization, "missing_user", "wrong_password")
        assert status == False
