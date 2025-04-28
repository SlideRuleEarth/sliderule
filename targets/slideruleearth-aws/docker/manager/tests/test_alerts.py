import json
from datetime import date, timedelta

def test_nominal(client):
    request = {
	    "record_time": "2024-04-21 14:30:00.345",
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    }
    response = client.post('/manager/alerts/issue', json=request)
    assert response.data == b'Alarm record successfully posted'

def test_value_counts(client):
    request = {
	    "record_time": "2024-04-21 14:30:00.345",
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    }
    # first alarm
    response = client.post('/manager/alerts/issue', json=request)
    assert response.data == b'Alarm record successfully posted'
    # second alarm
    response = client.post('/manager/alerts/issue', json=request)
    assert response.data == b'Alarm record successfully posted'
    # value counts - status_code
    response = client.get('/manager/status/alert_counts/status_code')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data["0"] == 2
    # value counts - version
    response = client.get('/manager/status/alert_counts/version')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data['v4.5.1'] == 2

def test_list(client):
    # first request - 1 year ago
    one_year_ago = (date.today() - timedelta(days=365)).strftime('%Y-%m-%d')
    response = client.post('/manager/alerts/issue', json={
        "record_time": f'{one_year_ago}',
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    })
    assert response.data == b'Alarm record successfully posted'
    # second request - 2 years ago
    two_years_ago = (date.today() - timedelta(days=730)).strftime('%Y-%m-%d')
    response = client.post('/manager/alerts/issue', json={
        "record_time": f'{two_years_ago}',
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    })
    assert response.data == b'Alarm record successfully posted'
    # third request - 3 years ago
    three_years_ago = (date.today() - timedelta(days=1095)).strftime('%Y-%m-%d')
    response = client.post('/manager/alerts/issue', json={
        "record_time": f'{three_years_ago}',
        "status_code": 0,
        "account": "sliderule",
        "version": "v4.5.1",
        "message": "success"
    })
    assert response.data == b'Alarm record successfully posted'
    # list - 1 year (and a day)
    response = client.get('/manager/status/alert_list?duration=31622400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 1
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    # list - 2 years (and a day)
    response = client.get('/manager/status/alert_list?duration=63158400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 2
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    # list - 3 years (and a day)
    response = client.get('/manager/status/alert_list?duration=94694400')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 3
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    assert data[2]['record_time'] == three_years_ago + ' 00:00:00'
    # list - between 1 and 3 years ago
    response = client.get(f'/manager/status/alert_list?t0=\'{three_years_ago}\'&t1=\'{one_year_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 3
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    assert data[2]['record_time'] == three_years_ago + ' 00:00:00'
    # list - between 2 and 3 years ago
    response = client.get(f'/manager/status/alert_list?t0=\'{two_years_ago}\'&t1=\'{one_year_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 2
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    assert data[1]['record_time'] == two_years_ago + ' 00:00:00'
    # list - between 1 year ago and now
    response = client.get(f'/manager/status/alert_list?t0=\'{one_year_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 1
    assert data[0]['record_time'] == one_year_ago + ' 00:00:00'
    # list - between 2 years ago and before
    response = client.get(f'/manager/status/alert_list?t1=\'{two_years_ago}\'')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert len(data) == 2
    assert data[0]['record_time'] == two_years_ago + ' 00:00:00'
    assert data[1]['record_time'] == three_years_ago + ' 00:00:00'