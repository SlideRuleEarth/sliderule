import pytest
import logging
import sliderule

softfail_reports = []

logging.basicConfig(level=logging.DEBUG)

def pytest_addoption(parser):
    parser.addoption("--domain", action="store", default="slideruleearth.io")
    parser.addoption("--organization", action="store", default="sliderule")
    parser.addoption("--desired_nodes", action="store", default=None)
    parser.addoption("--performance", action="store_true", default=False)

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
    return sliderule.init(domain, verbose=True, loglevel=logging.DEBUG, organization=organization, desired_nodes=desired_nodes)

@pytest.fixture(scope='session')
def performance(request):
    return request.config.option.performance

@pytest.hookimpl(hookwrapper=True)
def pytest_runtest_makereport(item, call):
    # Let pytest run and yield control to get the actual report
    outcome = yield
    report = outcome.get_result()
    # Mark the report as passed so pytest doesn't treat it as a failure
    if report.when == "call" and item.get_closest_marker("external") and report.failed:
        report.outcome = "passed"
        report.wasxfail = "external"
        softfail_reports.append((item.nodeid, call.excinfo.value))

def pytest_terminal_summary(terminalreporter):
    if softfail_reports:
        terminalreporter.write("\nEXTERNAL TEST FAILURES:\n", bold=True, yellow=True)
        for nodeid, exc in softfail_reports:
            terminalreporter.write_line(f"  {nodeid} - {type(exc).__name__}: {exc}")