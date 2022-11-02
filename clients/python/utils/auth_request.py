#
# Perform a authentication request
#

import sys
import json
import requests

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    username = sys.argv[1]
    password = sys.argv[2]
    organization = sys.argv[3]
    url = "slideruleearth.io"

    # Set URL
    if len(sys.argv) > 4:
        url = sys.argv[4]

    # Authentication Request
    host = "https://ps." + url + "/api/org_token/"
    rqst = {"username": username, "password": password, "org_name": organization}
    headers = {'Content-Type': 'application/json'}
    rsps = requests.post(host, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    refresh = rsps["refresh"]
    access = rsps["access"]
    print("Login Response: ", rsps)

    # Organization Access Request
    host = "https://ps." + url + "/api/membership_status/" + organization + "/"
    headers = {'Authorization': 'Bearer ' + access}
    rsps = requests.get(host, headers=headers, timeout=(60,10)).json()
    print("Validation Response: ", rsps)

    # Refresh Request
    host = "https://ps." + url + "/api/org_token/refresh/"
    rqst = {"refresh": refresh}
    headers = {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + access}
    rsps = requests.post(host, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    print("Refresh Response: ", rsps)
