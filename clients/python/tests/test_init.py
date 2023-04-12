"""Tests for sliderule-python connection errors when requests get sent back to back."""

import pytest
import sliderule
from sliderule import icesat2

@pytest.mark.network
class TestInit:
    def test_loop_init(self, domain, organization, desired_nodes):
        for _ in range(10):
            icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)

    def test_loop_versions(self, domain, organization, desired_nodes):
        icesat2.init(domain, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
        for _ in range(10):
            sliderule.source("version", {})
