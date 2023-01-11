"""Tests for sliderule-python connection errors when requests get sent back to back."""

import pytest
import sliderule
from sliderule import icesat2

@pytest.mark.network
class TestInit:
    def test_loop_init(self, domain, organization):
        for _ in range(10):
            icesat2.init(domain, organization=organization)

    def test_loop_versions(self, domain, organization):
        icesat2.init(domain, organization=organization)
        for _ in range(10):
            sliderule.source("version", {})
