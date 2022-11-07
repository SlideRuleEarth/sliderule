#
# Perform a authentication request
#

import sys
import json
import requests
import logging

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    username = sys.argv[1]
    password = sys.argv[2]
    organization = sys.argv[3]
    url = "slideruleearth.io"
    verbose = False

    # Set URL
    if len(sys.argv) > 4:
        url = sys.argv[4]

    # Set Verbose
    if len(sys.argv) > 5:
        verbose = sys.argv[5] == "verbose"

    # Configure Debug Logging
    if verbose:
        import http.client as http_client
        http_client.HTTPConnection.debuglevel = 1

        # You must initialize logging, otherwise you'll not see debug output.
        logging.basicConfig()
        logging.getLogger().setLevel(logging.DEBUG)
        requests_log = logging.getLogger("requests.packages.urllib3")
        requests_log.setLevel(logging.DEBUG)
        requests_log.propagate = True

    # Create Session
    session = requests.Session()
    session.trust_env = False

    # Authentication Request
    host = "https://ps." + url + "/api/org_token/"
    rqst = {"username": username, "password": password, "org_name": organization}
    headers = {'Content-Type': 'application/json'}
    rsps = session.post(host, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    refresh = rsps["refresh"]
    access = rsps["access"]
    print("Login Response: ", rsps)
    assert(len(refresh) > 0)
    assert(len(access) > 0)

    # Organization Access Request
    host = "https://ps." + url + "/api/membership_status/" + organization + "/"
    headers = {'Authorization': 'Bearer ' + access}
    rsps = session.get(host, headers=headers, timeout=(60,10)).json()
    print("Validation Response: ", rsps)
    assert(rsps["active"] == True)

    # Refresh Request 1
    host = "https://ps." + url + "/api/org_token/refresh/"
    rqst = {"refresh": refresh}
    headers = {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + access}
    rsps = session.post(host, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    refresh = rsps["refresh"]
    access = rsps["access"]
    print("Refresh 1 Response: ", rsps)
    assert(len(refresh) > 0)
    assert(len(access) > 0)

    # Refresh Request 2
    host = "https://ps." + url + "/api/org_token/refresh/"
    rqst = {"refresh": refresh}
    headers = {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + access}
    rsps = session.post(host, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    refresh = rsps["refresh"]
    access = rsps["access"]
    print("Refresh 2 Response: ", rsps)
    assert(len(refresh) > 0)
    assert(len(access) > 0)
