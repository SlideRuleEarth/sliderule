import json
import pytest

BEAMS = ["gt1l", "gt1r", "gt2l", "gt2r", "gt3l", "gt3r"]

def test_atl03_geoloc_corr(client):
    response = client.get('/ams/ATL03/geoloc_corr/ATL03_20181014113520_02420106_007_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert sorted(data.keys()) == BEAMS
    assert data["gt1l"]["shift_N_m"] == pytest.approx(-3.30338096038731)
    assert data["gt1l"]["shift_E_m"] == pytest.approx(3.41475110570104)
    assert data["gt1l"]["shift_U_m"] == pytest.approx(0.0026589705871545)
    assert data["gt3r"]["shift_N_m"] == pytest.approx(0.7052503102421356)
    assert data["gt3r"]["shift_E_m"] == pytest.approx(2.3098018578641315)
    assert data["gt3r"]["shift_U_m"] == pytest.approx(-0.0338793815994759)

def test_atl03_geoloc_corr_other_product(client):
    # non-ATL03 granule names are converted to the matching ATL03 granule
    atl03 = json.loads(client.get('/ams/ATL03/geoloc_corr/ATL03_20181014113520_02420106_007_01.h5').data.decode("utf-8"))
    response = client.get('/ams/ATL03/geoloc_corr/ATL06_20181014113520_02420106_007_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data == atl03

def test_atl03_geoloc_corr_null_shifts(client):
    response = client.get('/ams/ATL03/geoloc_corr/ATL03_20181014082645_02400106_007_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data == {}

def test_atl03_geoloc_corr_partial_null_shifts(client):
    response = client.get('/ams/ATL03/geoloc_corr/ATL03_20181014203635_02480102_007_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert 0 < len(data) < len(BEAMS)
    for beam, shifts in data.items():
        assert beam in BEAMS
        assert None not in shifts.values()

def test_atl03_geoloc_corr_unknown_granule(client):
    response = client.get('/ams/ATL03/geoloc_corr/ATL03_20000101000000_00000000_007_01.h5')
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data == {}

def test_atl03_geoloc_corr_sql_injection(client):
    response = client.get("/ams/ATL03/geoloc_corr/XXXXX' OR TRUE OR granule == '")
    data = json.loads(response.data.decode("utf-8"))
    assert response.status_code == 200
    assert data == {}
