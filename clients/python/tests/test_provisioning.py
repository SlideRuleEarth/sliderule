"""Tests for sliderule provisioning system api."""

import pytest
import sliderule
from sliderule.session import Session

class TestProvisioning:
    def test_authenticate(self, domain, organization):
        sliderule.set_url(domain)
        status = sliderule.authenticate(organization)
        assert status

    def test_refresh(self, domain, organization):
        headers = {}
        session = Session(domain, organization=organization)
        session._Session__buildauthheader(headers, force_refresh=True)
        assert len(headers['Authorization']) > 8

    # TO BE REMOVED
    # We cannot have a test that makes a capacity request
    # because the provisioning system will then automatically
    # schedule a teardown for that capacity even if the
    # resources were provisioned by cloudformation.
    #def test_num_nodes_update(self, domain, organization):
    #    sliderule.set_url(domain)
    #    status = sliderule.authenticate(organization)
    #    assert status
    #    result = sliderule.update_available_servers(2,20)
    #    assert len(result) == 2
    #    assert type(result[0]) == int
    #    assert type(result[1]) == int

    def test_bad_org(self, domain):
        sliderule.set_url(domain)
        status = sliderule.authenticate("non_existent_org")
        assert status == False

    def test_bad_creds(self, domain, organization):
        sliderule.set_url(domain)
        status = sliderule.authenticate(organization, "missing_user", "wrong_password")
        assert status == False
