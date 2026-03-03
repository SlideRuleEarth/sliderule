import pytest
import os

def pytest_addoption(parser):
    parser.addoption("--jwt_signing_key", action="store", default=None)

@pytest.fixture(autouse=True, scope="session")
def set_env_var(request):
    value = request.config.getoption("--jwt_signing_key")
    if value is not None:
        os.environ["JWT_SIGNING_KEY_ARN"] = value

