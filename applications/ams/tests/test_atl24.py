import json

def test_atl24_granule(client):
    response = client.get('/ams/atl24/granule/ATL24_20181014001920_02350103_006_02_002_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert data["gt1r"]["season"] == 3
    assert data["gt3r"]["bathy_photons"] == 16
    assert response.status_code == 200
    for beam in ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]:
        assert beam in data

def test_atl24_time_range(client):
    response = client.get('/ams/atl24?t0=2018-10-14&t1=2018-10-15')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 789
    assert len(data["granules"]) == 138
    assert len(data["granules"]["ATL24_20181014153336_02440113_006_02_002_01.h5"]) == 3
    for beam in ["gt1r", "gt2r", "gt3r"]:
        assert beam in data["granules"]["ATL24_20181014153336_02440113_006_02_002_01.h5"]

def test_atl24_season(client):
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 1507
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&season=0')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 289
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&season=1')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 296
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&season=2')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 449
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&season=3')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 473

def test_atl24_bathy_range(client):
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&photons0=100')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 859
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&meandepth1=10')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 844
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&mindepth1=10')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 1268
    response = client.get('/ams/atl24?t0=2019-09-30&t1=2019-10-02&maxdepth0=30')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 46

def test_atl24_poly(client):
    poly = [
        {"lat": 21.222261686673306, "lon": -73.78074797284968},
        {"lat": 21.07228912392266,  "lon": -73.78074797284968},
        {"lat": 21.07228912392266,  "lon": -73.51303956051089},
        {"lat": 21.222261686673306, "lon": -73.51303956051089},
        {"lat": 21.222261686673306, "lon": -73.78074797284968}
    ]
    poly_str = '%20'.join([f"{p['lon']}%20{p['lat']}" for p in poly])
    response = client.get(f'/ams/atl24?poly={poly_str}')
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 521
