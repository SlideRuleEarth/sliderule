# python

import sys
import json
import requests

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

def http_get(url, rqst):
    data = requests.get(url, data=json.dumps(rqst), timeout=(10,60))
    data.raise_for_status()
    lines = []
    for line in data.iter_content(None):
        lines.append(line)
    response = b''.join(lines)
    return json.loads(response)

def http_post(url, rqst):
    data = requests.post(url, data=json.dumps(rqst), timeout=(10,60))
    data.raise_for_status()
    lines = []
    for line in data.iter_content(None):
        lines.append(line)
    response = b''.join(lines)
    return json.loads(response)

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    server = "http://127.0.0.1:8050"
    if len(sys.argv) > 1:
        server = sys.argv[1]

    ###################
    # TEST - Health Check
    ###################

    rsps = http_get(server+"/health", {})
    assert rsps['health'] == True

    ###################
    # TEST - Repeated Posts
    ###################

    rsps = http_post(server+"/", {'service':'test', 'lifetime':60, 'name':'localhost'})
    assert rsps['localhost'][0] == 'test'
    rsps = http_post(server+"/", {'service':'test', 'lifetime':60, 'name':'localhost'})
    assert rsps['localhost'][0] == 'test'
    rsps = http_post(server+"/", {'service':'test', 'lifetime':60, 'name':'localhost'})
    assert rsps['localhost'][0] == 'test'
