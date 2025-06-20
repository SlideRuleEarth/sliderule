import os
import tempfile

import pytest
from manager import create_app
from manager.db import execute_file_db, close_db

@pytest.fixture
def app():
    tmpname = os.path.join(tempfile.gettempdir(), next(tempfile._get_candidate_names()))

    app = create_app({
        'TESTING': True,
        'DATABASE': tmpname,
        'SECRET_SALT': '',
        'API_KEY': '',
        'GEOLITE2_ASN': '/data/GeoLite2-ASN.mmdb',
        'GEOLITE2_CITY': '/data/GeoLite2-City.mmdb',
        'GEOLITE2_COUNTRY': '/data/GeoLite2-Country.mmdb',
        'ATL13_MAPPINGS': '/data/ATL13/atl13_mappings.json',
        'ATL13_MASK': '/data/ATL13/inland_water_body.parquet',
        'ENDPOINT_TLM_EXCLUSION': [],
        'ORCHESTRATOR': "http:127.0.0.1:8050"
    })

    with app.app_context():
        execute_file_db('schema.sql')

    yield app

    with app.app_context():
        close_db()

    os.unlink(tmpname)

@pytest.fixture
def client(app):
    return app.test_client()

@pytest.fixture
def runner(app):
    return app.test_cli_runner()