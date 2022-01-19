import pytest

def pytest_addoption(parser):
    parser.addoption("--server", action="store", default="icesat2sliderule.org")
    parser.addoption("--asset", action="store", default="nsidc-s3")

#def pytest_generate_tests(metafunc):
#    # This is called for every test. Only get/set command line arguments
#    # if the argument is specified in the list of test "fixturenames".
#    option_value = metafunc.config.option.server
#    if 'server' in metafunc.fixturenames and option_value is not None:
#        metafunc.parametrize("server", [option_value])

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
