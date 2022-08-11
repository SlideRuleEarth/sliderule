# python

import sys
import json
import ctypes
import requests
from time import sleep

###############################################################################
# UTILITY FUNCTIONS
###############################################################################

def http_get(url, rqst, as_json=True):
    data = requests.get(url, data=json.dumps(rqst), timeout=(10,60))
    data.raise_for_status()
    lines = []
    for line in data.iter_content(None):
        lines.append(line)
    response = b''.join(lines)
    if as_json:
        return json.loads(response)
    else:
        return ctypes.create_string_buffer(response).value.decode('ascii')

def http_post(url, rqst, as_json=True):
    data = requests.post(url, data=json.dumps(rqst), timeout=(10,60))
    data.raise_for_status()
    lines = []
    for line in data.iter_content(None):
        lines.append(line)
    response = b''.join(lines)
    if as_json:
        return json.loads(response)
    else:
        return ctypes.create_string_buffer(response).value.decode('ascii')

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    server = "127.0.0.1"
    if len(sys.argv) > 1:
        server = sys.argv[1]
    url = "http://" + server + ":8050/discovery/"
    scrub_interval = 5
    num_locks_per_node = 3

    ###################
    # TEST - Health Check
    ###################

    rsps = http_get(url+"health", {})
    assert rsps['health'] == True

    ###################
    # TEST - Repeated Posts
    ###################

    rsps = http_post(url, {'service':'test', 'lifetime':1, 'name':'localhost'})
    assert rsps['localhost'][0] == 'test'
    rsps = http_post(url, {'service':'test', 'lifetime':1, 'name':'localhost'})
    assert rsps['localhost'][0] == 'test'
    rsps = http_post(url, {'service':'test', 'lifetime':1, 'name':'localhost'})
    assert rsps['localhost'][0] == 'test'

    sleep(2)

    ###################
    # TEST - Get Member
    ###################

    rsps = http_post(url, {'service':'test', 'lifetime':2, 'name':'bob'})
    assert rsps['bob'][0] == 'test'
    rsps = http_get(url+"lock", {'service':'test', 'nodesNeeded': 1, 'timeout': 1})
    assert rsps['members'][0] == 'bob'
    transactions = rsps['transactions']
    rsps = http_get(url+"unlock", {'transactions':[transactions[0]]})
    assert rsps['complete'] == 1

    ###################
    # TEST - Expire Member
    ###################

    rsps = http_post(url, {'service':'test', 'lifetime':1, 'name':'bob'})
    assert rsps['bob'][0] == 'test'
    sleep(2 + scrub_interval)
    rsps = http_get(url+"lock", {'service':'test', 'nodesNeeded': 1, 'timeout': 5})
    assert len(rsps['members']) == 0
    assert len(rsps['transactions']) == 0

    ###################
    # TEST - Expire Transaction
    ###################

    rsps = http_post(url, {'service':'test', 'lifetime':(scrub_interval + 10), 'name':'bob'})
    assert rsps['bob'][0] == 'test'
    rsps = http_get(url+"lock", {'service':'test', 'nodesNeeded': num_locks_per_node, 'timeout': 1})
    assert len(rsps['members']) == num_locks_per_node
    assert len(rsps['transactions']) == num_locks_per_node
    assert rsps['members'][0] == 'bob'
    # no nodes should be available at this point
    rsps = http_get(url+"lock", {'service':'test', 'nodesNeeded': 1, 'timeout': 1})
    assert len(rsps['members']) == 0
    assert len(rsps['transactions']) == 0
    # wait for transactions to expire
    sleep(1 + scrub_interval)
    rsps = http_get(url+"lock", {'service':'test', 'nodesNeeded': 1, 'timeout': 1})
    assert len(rsps['members']) == 1
    assert len(rsps['transactions']) == 1
    assert rsps['members'][0] == 'bob'

    ###################
    # TEST - Prometheus
    ###################

    rsps = http_get(url+"prometheus", {}, as_json=False)
    print(rsps)
    metrics = {}
    for line in rsps.splitlines():
        if len(line.strip()) > 0 and line[0] != '#':
            elements = line.split()
            metrics[elements[0]] = int(elements[1])
    assert metrics['num_requests'] > 0
    assert metrics['num_complete'] > 0
    assert metrics['num_failures'] == 0
    assert metrics['num_timeouts'] > 0
    assert metrics['num_active_locks'] == 1
    assert metrics['test_members'] == 1
