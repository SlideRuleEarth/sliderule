import json

def test_atl13_refid(client):
    response = client.get('/manager/ams/atl13?refid=5952002394')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data["source"] == "GRWL"

def test_atl13_name(client):
    response = client.get('/manager/ams/atl13?name=Caspian%20Sea')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data["source"] == "HydroLAKES"

def test_atl13_coord(client):
    response = client.get('/manager/ams/atl13?lon=-86.79835088109307&lat=42.762733124439904')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data["source"] == "HydroLAKES"
