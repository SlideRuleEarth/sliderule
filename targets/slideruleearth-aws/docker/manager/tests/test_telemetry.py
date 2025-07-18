import json
from datetime import date, datetime, timedelta

def test_nominal(client):
    request = {
	    "record_time": "2024-04-21 14:30:00.345",
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    }
    response = client.post('/manager/telemetry/record', json=request)
    assert response.data == b'Telemetry record successfully posted'

def test_value_counts(client):
    request = {
        "record_time": "2024-04-21 14:30:00.345",
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    }
    # first request
    response = client.post('/manager/telemetry/record', json=request)
    assert response.data == b'Telemetry record successfully posted'
    # second request
    response = client.post('/manager/telemetry/record', json=request)
    assert response.data == b'Telemetry record successfully posted'
    # value counts - endpoint
    response = client.get('/manager/status/telemetry_counts/endpoint')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data['defaults'] == 2
    # value counts - version
    response = client.get('/manager/status/telemetry_counts/version')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data['v4.5.1'] == 2
    # value counts - source_ip_hash
    response = client.get('/manager/status/telemetry_counts/source_ip_hash')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    for key in data:
        assert data[key] == 2

def test_list(client):
    # first request - 1 year ago
    one_year_ago = (date.today() - timedelta(days=365)).strftime('%Y-%m-%d')
    response = client.post('/manager/telemetry/record', json={
        "record_time": f'{one_year_ago}',
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    })
    assert response.data == b'Telemetry record successfully posted'
    # second request - 2 years ago
    two_years_ago = (date.today() - timedelta(days=730)).strftime('%Y-%m-%d')
    response = client.post('/manager/telemetry/record', json={
        "record_time": f'{two_years_ago}',
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    })
    assert response.data == b'Telemetry record successfully posted'
    # third request - 3 years ago
    three_years_ago = (date.today() - timedelta(days=1095)).strftime('%Y-%m-%d')
    response = client.post('/manager/telemetry/record', json={
        "record_time": f'{three_years_ago}',
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    })
    assert response.data == b'Telemetry record successfully posted'
    # list - 1 year (and a day)
    response = client.get('/manager/status/telemetry_list?duration=31622400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 1
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    # list - 2 years (and a day)
    response = client.get('/manager/status/telemetry_list?duration=63158400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 2
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    # list - 3 years (and a day)
    response = client.get('/manager/status/telemetry_list?duration=94694400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 3
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    assert data[2]['record_time'] == three_years_ago + ' 00:00:00'
    # list - between 1 and 3 years ago
    response = client.get(f'/manager/status/telemetry_list?t0=\'{three_years_ago}\'&t1=\'{one_year_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 3
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    assert data[2]['record_time'] == three_years_ago + ' 00:00:00'
    # list - between 2 and 3 years ago
    response = client.get(f'/manager/status/telemetry_list?t0=\'{two_years_ago}\'&t1=\'{one_year_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 2
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    # list - between 1 year ago and now
    response = client.get(f'/manager/status/telemetry_list?t0=\'{one_year_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 1
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    # list - between 2 years ago and before
    response = client.get(f'/manager/status/telemetry_list?t1=\'{two_years_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 2
    assert data[0]['record_time'] == two_years_ago + ' 00:00:00'
    assert data[1]['record_time'] == three_years_ago + ' 00:00:00'

def test_timespan(client):
    # first request - now
    now = date.today()
    response = client.post('/manager/telemetry/record', json={
        "record_time": f'{now.strftime('%Y-%m-%d')}',
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    })
    assert response.data == b'Telemetry record successfully posted'
    # second request - 2 years ago
    two_years_ago = date.today() - timedelta(days=730)
    response = client.post('/manager/telemetry/record', json={
        "record_time": f'{two_years_ago.strftime('%Y-%m-%d')}',
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    })
    assert response.data == b'Telemetry record successfully posted'
    # list - 1 year (and a day)
    response = client.get('/manager/status/timespan/record_time')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    start = datetime.fromisoformat(data["start"])
    end = datetime.fromisoformat(data["end"])
    span = timedelta(seconds=data["span"])
    assert start.date() == two_years_ago
    assert end.date() == now
    assert span == now - two_years_ago

def test_geo(client):
    response = client.post('/manager/telemetry/record', json={
        "record_time": f'{date.today().strftime('%Y-%m-%d')}',
        "source_ip": "128.154.178.80",
        "aoi": {"x": 30.0, "y": 57.0},
        "client": "pytest",
        "endpoint":"defaults",
        "duration": 1.0,
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1"
    })
    assert response.data == b'Telemetry record successfully posted'
    response = client.get(f'/manager/status/telemetry_list?duration=86400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 1
    assert data[0]['source_ip_location'] == "United States, Catonsville"