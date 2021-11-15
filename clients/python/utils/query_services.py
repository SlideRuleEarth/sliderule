#
# Query the consul server and display the available services
#

import sys
import requests
import json

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Set URL #
    ipaddr = "icesat2sliderule.org"
    if len(sys.argv) > 1:
        ipaddr = sys.argv[1]

    # Set Service #
    service = "sliderule"
    if len(sys.argv) > 2:
        service = sys.argv[2]

    # Query Services #
    response = requests.get("http://"+ipaddr+":8050", data='{"service":"%s"}'%(service)).json()

    # Display Services #
    print(json.dumps(response, indent=2))
    print("Number of Nodes: {}".format(len(response['members'])))
