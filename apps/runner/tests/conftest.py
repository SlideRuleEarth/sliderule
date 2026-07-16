import pytest

def pytest_addoption(parser):
    parser.addoption("--username", action="store", default=None)

@pytest.fixture(scope='session')
def username(request):
    value = request.config.option.username
    if value is None:
        pytest.skip()
    return value
