import pytest
from ams import create_app

@pytest.fixture
def app():
    app = create_app({
        'TESTING': True,
        'ATL13_MAPPINGS': '/data/atl13.json',
        'ATL13_MASK': '/data/atl13.db',
        'ATL24_DB': '/data/atl24r2.db',
        'USGS3DEP_DB': '/data/3dep.db',
        'CASALS1B_DB': '/data/casals1b.db',
        'CASALS1B_SEARCH_MASK': '/data/casals1b.geojson',
        'MAX_RESOURCES': 10000
    })
    yield app

@pytest.fixture
def client(app):
    return app.test_client()

@pytest.fixture
def runner(app):
    return app.test_cli_runner()