import pytest
from ams import create_app

@pytest.fixture
def app():
    app = create_app({
        'TESTING': True,
        'ATL13_MAPPINGS': '/data/ATL13/atl13.json',
        'ATL13_MASK': '/data/ATL13/atl13.db',
        'ATL24_DB': '/data/ATL24/atl24r2.db',
        'USGS3DEP_DB': '/data/3DEP/3dep.db',
        'MAX_RESOURCES': 10000
    })
    yield app

@pytest.fixture
def client(app):
    return app.test_client()

@pytest.fixture
def runner(app):
    return app.test_cli_runner()