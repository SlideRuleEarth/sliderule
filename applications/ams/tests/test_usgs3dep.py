import json

def test_3dep_id(client):
    response = client.get('/ams/3dep/id/USGS_1M_10_x56y524_WA_KingCounty_2021_B21')
    data = json.loads(response.data.decode("utf-8"))
    assert data["id"] == 'USGS_1M_10_x56y524_WA_KingCounty_2021_B21'
    assert data["assets"]["elevation"]["href"] == 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/1m/Projects/WA_KingCounty_2021_B21/TIFF/USGS_1M_10_x56y524_WA_KingCounty_2021_B21.tif'
    assert data["assets"]["elevation"]["raster:bands"][0]["data_type"] == 'float32'

def test_3dep_time_range(client):
    response = client.post('/ams/3dep', json={"t0":"2018-10-14", "t1":"2018-10-15"})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 23
    assert data["features"][0]["id"] == "USGS_one_meter_x46y440_UT_Sanpete_2018"

def test_dep_poly(client):
    poly = [
        {"lon": -107.88014597969254, "lat": 38.86682337659025},
        {"lon": -107.88014597969254, "lat": 39.106829776489064},
        {"lon": -108.22632700395818, "lat": 39.106829776489064},
        {"lon": -108.22632700395818, "lat": 38.86682337659025},
        {"lon": -107.88014597969254, "lat": 38.86682337659025}
    ]
    response = client.post(f'/ams/3dep', json={"poly":poly})
    data = json.loads(response.data.decode("utf-8"))
    assert data["hits"] == 11
    assert data["features"][0]["id"] == "USGS_one_meter_x25y434_CO_MesaCo_QL1_2016"
