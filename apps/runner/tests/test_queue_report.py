from runner import report_queue_handler
import pytest

def test_missing_job_state():
    with pytest.raises(KeyError):
        rsps = report_queue_handler({})

def test_job_state_wrong_type():
    with pytest.raises(RuntimeError):
        rsps = report_queue_handler({"job_state": 1})

def test_job_state_invalid():
    with pytest.raises(RuntimeError):
        rsps = report_queue_handler({"job_state": ["RUNNING", "WRONG"]})
