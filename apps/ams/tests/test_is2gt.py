import json

def test_find(client):
    response = client.post('/ams/IS2GT', json={
        "lon": -105.0,
        "lat": 39.5,
        "r": 20,
        "u": "miles"
    })
    assert response.status_code == 200, response.get_data(as_text=True)
    data = json.loads(response.data.decode("utf-8"))
    assert "hits" in data

#def test_legacy(client):
#    response = client.post('/ams/IS2GT/legacy', json={
#        "lon": -105.0,
#        "lat": 39.5,
#        "r": 20,
#        "u": "miles"
#    })
#    assert response.status_code == 200, response.get_data(as_text=True)
#    data = json.loads(response.data.decode("utf-8"))
#    assert data["state"]
#    print(data)
