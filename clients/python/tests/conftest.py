import pytest

def pytest_addoption(parser):
    parser.addoption("--domain", action="store", default="slideruleearth.io")
    parser.addoption("--asset", action="store", default="icesat2")
    parser.addoption("--organization", action="store", default="sliderule")
    parser.addoption("--desired_nodes", action="store", default=None)

@pytest.fixture(scope='session')
def domain(request):
    value = request.config.option.domain
    if value is None:
        pytest.skip()
    return value

@pytest.fixture(scope='session')
def asset(request):
    value = request.config.option.asset
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
