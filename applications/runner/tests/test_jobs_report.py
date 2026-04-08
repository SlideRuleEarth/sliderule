from runner import report_jobs_handler
import pytest
import json

def test_missing_job_list():
    with pytest.raises(KeyError):
        rsps = report_jobs_handler({})

def test_job_list_wrong_type():
    with pytest.raises(RuntimeError):
        rsps = report_jobs_handler({"job_list": "my list"})

def test_job_list_too_big():
    with pytest.raises(RuntimeError):
        rsps = report_jobs_handler({"job_list": [x for x in range(101)]})

def test_non_existant_job_id():
    rsps = report_jobs_handler({"job_list": ["does_not_exist"]})
    print(rsps)
    assert len(json.loads(rsps['body'])) == 0
