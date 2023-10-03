"""Tests for Lua endpoint failures."""

import pytest
import sliderule
from sliderule import icesat2

def catchlogs(rec):
    global GLOBAL_message
    GLOBAL_message = rec["attr"]

def catchexceptions(rec):
    global GLOBAL_message
    GLOBAL_message = rec["text"]

GLOBAL_message = ""
GLOBAL_callbacks = {'eventrec': catchlogs, 'exceptrec': catchexceptions}

@pytest.mark.network
class TestAtl03s:
    def test_badasset(self, init):
        invalid_asset = "invalid-asset"
        rqst = {
            "resource": [],
            "parms": {"asset" : "invalid-asset"}
        }
        rsps = sliderule.source("atl03s", rqst, stream=True, callbacks=GLOBAL_callbacks)
        assert init
        assert(len(rsps) == 0)
        assert("invalid asset specified: {}".format(invalid_asset) == GLOBAL_message)

@pytest.mark.network
class TestAtl06:
    def test_badasset(self, init):
        invalid_asset = "invalid-asset"
        rqst = {
            "resource": [],
            "parms": {"asset" : "invalid-asset"}
        }
        rsps = sliderule.source("atl06", rqst, stream=True, callbacks=GLOBAL_callbacks)
        assert init
        assert(len(rsps) == 0)
        assert("invalid asset specified: {}".format(invalid_asset) == GLOBAL_message)

    def test_timeout(self, init):
        resource = "ATL03_20220208000041_07291401_005_01.h5"
        rqst = {
            "resource": resource,
            "parms": {"track": 0, "srt": 0, "pass_invalid":True, "yapc": {"score":0}, "timeout": 1},
        }
        rsps = sliderule.source("atl06", rqst, stream=True, callbacks=GLOBAL_callbacks)
        assert init
        assert(len(rsps) == 0)
        # assert("{} timed-out after 10 seconds".format(resource) in GLOBAL_message) # non-deterministic
