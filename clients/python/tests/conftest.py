import pytest
import logging
import sliderule

logging.basicConfig(level=logging.DEBUG)

def pytest_addoption(parser):
    parser.addoption("--domain", action="store", default="slideruleearth.io")
    parser.addoption("--organization", action="store", default="sliderule")
    parser.addoption("--desired_nodes", action="store", default=None)

@pytest.fixture(scope='session')
def domain(request):
    value = request.config.option.domain
    if value is None:
        pytest.skip()
    return value

@pytest.fixture(scope='session')
def organization(request):
    value = request.config.option.organization
    if value == "None":
        value = None
    return value

@pytest.fixture(scope='session')
def desired_nodes(request):
    value = request.config.option.desired_nodes
    if value is not None:
        if value == "None":
            value = None
        else:
            value = int(value)
    return value

@pytest.fixture(scope='function')
def init(domain, organization, desired_nodes):
    return sliderule.init(domain, verbose=True, loglevel=logging.DEBUG, organization=organization, desired_nodes=desired_nodes, bypass_dns=True)
