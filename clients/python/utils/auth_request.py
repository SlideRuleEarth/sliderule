#
# Perform a authentication request
#

import json
import requests

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Authentication Request
    url = "https://ps.testsliderule.org/ps/api/org_token/"
    rqst = {"username": "<username>", "password": "<password>", "org_name": "<organization>"}
    headers = {'Content-Type': 'application/json'}
    rsps = requests.post(url, data=json.dumps(rqst), headers=headers, timeout=(60,10)).json()
    refresh = rsps["refresh"]
    access = rsps["access"]
    print("My Token: ", access)

    # Organization Access Request
    url = "https://ps.testsliderule.org/ps/api/get_membership_status/" + "Developers"
    headers = {'Authorization': 'Bearer ' + access}
    rsps = requests.get(url, headers=headers, timeout=(60,10)).json()
    print(rsps)