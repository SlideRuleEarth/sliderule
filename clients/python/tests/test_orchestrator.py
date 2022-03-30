import pytest
import requests
import concurrent.futures
#import sliderule
#from sliderule import icesat2

SIMULTANEOUS_COUNT = 50
TOTAL_COUNT = 1000
SERVICE = "sliderule"

def service_query(url, service):
    response = requests.get("http://"+url+"/discovery/", data='{"service":"%s"}'%(service))
    return response.json()

@pytest.mark.network
class TestOrchestrator:
    
    def test_rate(self, server):
        status = True
        with concurrent.futures.ThreadPoolExecutor(max_workers=SIMULTANEOUS_COUNT) as executor:
            futures = [executor.submit(service_query, server, SERVICE) for _ in range(TOTAL_COUNT)]
            for future in concurrent.futures.as_completed(futures):
                members = future.result()['members']
                if len(members) <= 0:
                    status = False
        assert status

    def test_postfix(self, server):
        for path in ["/d", "/asdf", "/e/d/a/"]:
            response = requests.get("http://"+server+"/discovery"+path, data='{"service":"%s"}'%("sliderule"))
            assert response.status_code == 200

    def test_prefix(self, server):
        for path in ["/../../d", "/../asdf"]:
            response = requests.get("http://"+server+"/discovery"+path, data='{"service":"%s"}'%("sliderule"))
            assert response.status_code == 404
        
    def test_timeout(self, server):
        with pytest.raises(requests.exceptions.ConnectTimeout):
            for path in [":3324"]:
                requests.get("http://"+server+path, data='{"service":"%s"}'%("sliderule"), timeout=(5,5))
