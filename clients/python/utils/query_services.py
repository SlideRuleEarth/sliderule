# python

import sys
import requests
import json

###############################################################################
# APIs
###############################################################################

#
#  QUERY SERVICES
#
def query_services (url, service, list_only=False, passing_only=False):

    # Build Request String #
    rqst_str = url + "/v1/catalog/service/" + service
    if passing_only:
        rqst_str += "?passing"

    # Query Services #
    services = requests.get(rqst_str).json()

    # Pull Out IPs #
    if list_only:
        ip_list = []
        for entry in services:
            ip_list.append(entry["Node"])
        services = ip_list

    return services

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Set URL #
    url = "http://34.222.95.172:8500"
    if len(sys.argv) > 1:
        url = sys.argv[1]

    # Set Service #
    service = "srds"
    if len(sys.argv) > 2:
        service = sys.argv[2]
    
    # Set Listing Options #
    list_only = False
    if len(sys.argv) > 3:
        if sys.argv[3] == "True":
            list_only = True

    # Set Query Options #
    passing_only = False
    if len(sys.argv) > 4:
        if sys.argv[4] == "Passing":
            passing_only = True

    # Query Services #
    services = query_services(url, service, list_only, passing_only)

    # Display Services #
    print(json.dumps(services, indent=2))
