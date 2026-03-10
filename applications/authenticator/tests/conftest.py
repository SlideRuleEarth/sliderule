import time
import multiprocessing
import uvicorn
import pytest

def run_stub():
    uvicorn.run("github_stub:app", host="0.0.0.0", port=9083)

@pytest.fixture(scope="session", autouse=True)
def github_stub_server():
    proc = multiprocessing.Process(target=run_stub, daemon=True)
    proc.start()
    time.sleep(1)  # give it a moment to start
    yield
    proc.terminate()
    proc.join()