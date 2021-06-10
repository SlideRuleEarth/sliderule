#
# Query the consul server and display the available services
#

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
            ip_list.append(entry["Address"])
        services = ip_list

    return services

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Set URL #
    ipaddr = "icesat2sliderule.org"
    if len(sys.argv) > 1:
        ipaddr = sys.argv[1]

    # Set Listing Options #
    list_only = False
    if len(sys.argv) > 2:
        if sys.argv[2] == "List":
            list_only = True

    # Set Query Options #
    passing_only = False
    if len(sys.argv) > 3:
        if sys.argv[3] == "Passing":
            passing_only = True

    # Set Service #
    service = "srds"
    if len(sys.argv) > 4:
        service = sys.argv[4]

    # Query Services #
    services = query_services("http://"+ipaddr+":8500", service, list_only, passing_only)

    # Display Services #
    print(json.dumps(services, indent=2))
    print("Number of Nodes: {}".format(len(services)))
