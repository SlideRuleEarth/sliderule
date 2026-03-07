from runner import report_queue_handler

def test_missing_job_state():
    rsps = report_queue_handler({}, None)
    assert rsps['statusCode'] == 500

def test_job_state_wrong_type():
    rsps = report_queue_handler({"job_state": 1}, None)
    assert rsps['statusCode'] == 500

def test_job_state_invalid():
    rsps = report_queue_handler({"job_state": ["RUNNING", "WRONG"]}, None)
    assert rsps['statusCode'] == 500
