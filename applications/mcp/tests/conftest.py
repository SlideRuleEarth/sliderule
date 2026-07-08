import pytest
import sliderule
import os

sliderule.init("localhost", organization=None)

os.environ.setdefault("CLUSTER", "sliderule")
os.environ.setdefault("PROJECT_PUBLIC_BUCKET", "sliderule-public")

def pytest_addoption(parser):
    parser.addoption("--local", action="store_true", default=False)

@pytest.fixture(scope='session')
def local(request):
    value = request.config.getoption("--local")
    if not value:
        pytest.skip("requires --local")
    return value
