"""Tests for sliderule xdem coregistration."""

import pytest
from sliderule import sliderule

@pytest.mark.network
class TestXDEM:

    def test_nominal(self, init):
        parms = {
            "fn_epc": "asdf",
            "fn_dem": "asdf"
        }
        results = sliderule.source("xdem", parms)
        assert init
        return results


# ########################################################
# Main
# ########################################################

if __name__ == '__main__':

    init_status = sliderule.init("localhost", organization=None, verbose=True)

    test = TestXDEM()
    results = test.test_nominal(init_status)

    print(results)