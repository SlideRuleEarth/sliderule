import os
import tempfile

import pytest
from manager import create_app
from manager.db import execute_db, close_db

@pytest.fixture
def app():
    tmpname = os.path.join(tempfile.gettempdir(), next(tempfile._get_candidate_names()))

    app = create_app({
        'TESTING': True,
        'DATABASE': tmpname,
         'SECRET_SALT': '',
         'GEOLITE2_ASN': '/data/GeoLite2-ASN_20250424/GeoLite2-ASN.mmdb',
         'GEOLITE2_CITY': '/data/GeoLite2-City_20250422/GeoLite2-City.mmdb',
         'GEOLITE2_COUNTRY': '/data/GeoLite2-Country_20250422/GeoLite2-Country.mmdb'
    })

    with app.app_context():
        execute_db('schema.sql')

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