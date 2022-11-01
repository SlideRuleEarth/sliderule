import pytest

def pytest_addoption(parser):
    parser.addoption("--server", action="store", default="slideruleearth.io")
    parser.addoption("--asset", action="store", default="nsidc-s3")
    parser.addoption("--organization", action="store", default="sliderule")

@pytest.fixture(scope='session')
def server(request):
    server_value = request.config.option.server
    if server_value is None:
        pytest.skip()
    return server_value

@pytest.fixture(scope='session')
def asset(request):
    asset_value = request.config.option.asset
    if asset_value is None:
        pytest.skip()
    return asset_value

@pytest.fixture(scope='session')
def organization(request):
    organization_value = request.config.option.organization
    if organization_value == "None":
        organization_value = None
    return organization_value
