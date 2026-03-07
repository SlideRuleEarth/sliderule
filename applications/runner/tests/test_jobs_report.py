from runner import report_jobs_handler
import json

def test_missing_job_list():
    rsps = report_jobs_handler({})
    assert rsps['statusCode'] == 500

def test_job_list_wrong_type():
    rsps = report_jobs_handler({"job_list": "my list"})
    assert rsps['statusCode'] == 500

def test_job_list_too_big():
    rsps = report_jobs_handler({"job_list": [x for x in range(101)]})
    assert rsps['statusCode'] == 500

def test_non_existant_job_id():
    rsps = report_jobs_handler({"job_list": ["does_not_exist"]})
    print(rsps)
    assert len(json.loads(rsps['body'])) == 0
