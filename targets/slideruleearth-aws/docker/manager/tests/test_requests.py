import json

def test_nominal(client):
    request = {
	    "request_time": "2024-04-21 14:30:00.345",
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "organization": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    }
    response = client.post('/metrics/record_request', json=request)
    assert response.data == b'Request record successfully posted'

def test_value_counts(client):
    request = {
        "request_time": "2024-04-21 14:30:00.345",
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "organization": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    }
    # first request
    response = client.post('/metrics/record_request', json=request)
    assert response.data == b'Request record successfully posted'
    # second request
    response = client.post('/metrics/record_request', json=request)
    assert response.data == b'Request record successfully posted'
    # value counts - endpoint
    response = client.get('/status/request_counts/endpoint')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data['defaults'] == 2
    # value counts - version
    response = client.get('/status/request_counts/version')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data['v4.5.1'] == 2
    # value counts - source_ip_hash
    response = client.get('/status/request_counts/source_ip_hash')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    for key in data:
        assert data[key] == 2
