import json

def test_atl13_refid(client):
    refid = 5952002394
    response = client.post('/ams/ATL13', json={"refid":refid})
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data["refid"] == refid
    assert 'ATL13_20181014031222_02370101_007_01.h5' in data["granules"]
    assert len(data["granules"]) == 51, len(data["granules"])

def test_atl13_name(client):
    name = "Caspian Sea"
    response = client.get('/ams/ATL13', json={"name": name})
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert 'ATL13_20181014220350_02490101_007_01.h5' in data["granules"], data["granules"]
    assert len(data["granules"]) == 1569, len(data["granules"])

def test_atl13_coord(client):
    response = client.get('/ams/ATL13', json={"coord": {"lon":-86.79835088109307, "lat":42.762733124439904}})
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert 'ATL13_20200417014548_03290701_007_01.h5' in data["granules"], data["granules"]
    assert len(data["granules"]) == 594, len(data["granules"])
