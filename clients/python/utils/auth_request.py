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

    # Authentication Request
    url = "https://ps.testsliderule.org/ps/api/org_token/"
    rqst = {"username": username, "password": password, "org_name": organization}
    headers = {'Content-Type': 'application/json'}
    rsps = requests.post(url, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    print("PS Response: ", rsps)
    refresh = rsps["refresh"]
    access = rsps["access"]
    print("My Token: ", access)

    # Organization Access Request
    url = "https://ps.testsliderule.org/ps/api/get_membership_status/" + organization + "/"
    headers = {'Authorization': 'Bearer ' + access}
    rsps = requests.get(url, headers=headers, timeout=(60,10)).json()
    print(rsps)