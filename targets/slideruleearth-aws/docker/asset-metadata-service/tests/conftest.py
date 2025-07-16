import pytest
from ams import create_app

@pytest.fixture
def app():
    app = create_app({
        'TESTING': True,
        'ATL13_MAPPINGS': '/data/ATL13/atl13_mappings.json',
        'ATL13_MASK': '/data/ATL13/inland_water_body.db',
    })
    yield app

@pytest.fixture
def client(app):
    return app.test_client()

@pytest.fixture
def runner(app):
    return app.test_cli_runner()