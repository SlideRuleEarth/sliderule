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
