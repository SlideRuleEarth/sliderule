from runner import report_jobs_handler

def test_missing_job_list():
    rsps = report_jobs_handler({}, None)
    assert rsps["status"] == False

def test_job_list_wrong_type():
    rsps = report_jobs_handler({"job_list": "my list"}, None)
    assert rsps["status"] == False
    assert rsps["exception"] == 'User error in job report'

def test_job_list_too_big():
    rsps = report_jobs_handler({"job_list": [x for x in range(101)]}, None)
    assert rsps["status"] == False
    assert rsps["exception"] == 'User error in job report'

def test_non_existant_job_id():
    rsps = report_jobs_handler({"job_list": ["does_not_exist"]}, None)
    assert len(rsps["report"]) == 0
