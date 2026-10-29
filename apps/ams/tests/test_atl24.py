import json

def test_atl24_granule(client):
    response = client.get('/ams/ATL24/granule/ATL24_20181014001920_02350103_007_01_003_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert data["gt1r"]["season"] == 3
    assert data["gt3r"]["bathy_photons"] == 21
    assert response.status_code == 200
    for beam in ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]:
        assert beam in data

def test_atl24_time_range(client):
    response = client.post('/ams/ATL24', json={"t0":"2018-10-14", "t1":"2018-10-15"})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 848
    assert len(data["granules"]) == 147
    assert "ATL24_20181014001920_02350103_007_01_003_01.h5" in data["granules"]

def test_atl24_season(client):
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02"})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 1597
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "season":0})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 0
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "season":1})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 674
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "season":2})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 0
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "season":3})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 923

def test_atl24_bathy_range(client):
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "photons0":100})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 1091
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1": "2019-10-02", "meandepth1":10})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 1153
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "mindepth1":10})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 1283
    response = client.post('/ams/ATL24', json={"t0":"2019-09-30", "t1":"2019-10-02", "maxdepth0":30})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 178

def test_atl24_poly(client):
    poly = [
        {"lat": 21.222261686673306, "lon": -73.78074797284968},
        {"lat": 21.07228912392266,  "lon": -73.78074797284968},
        {"lat": 21.07228912392266,  "lon": -73.51303956051089},
        {"lat": 21.222261686673306, "lon": -73.51303956051089},
        {"lat": 21.222261686673306, "lon": -73.78074797284968}
    ]
    response = client.post(f'/ams/ATL24', json={"poly":poly})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 510
