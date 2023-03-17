import pytest

def pytest_addoption(parser):
    parser.addoption("--domain", action="store", default="slideruleearth.io")
    parser.addoption("--asset", action="store", default="nsidc-s3")
    parser.addoption("--organization", action="store", default="sliderule")
    parser.addoption("--desired_nodes", action="store", default=None)

@pytest.fixture(scope='session')
def domain(request):
    domain_value = request.config.option.domain
    if domain_value is None:
        pytest.skip()
    return domain_value

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

@pytest.fixture(scope='session')
def desired_nodes(request):
    desired_nodes_value = request.config.option.desired_nodes
    if desired_nodes == "None":
        desired_nodes = None
    return desired_nodes
