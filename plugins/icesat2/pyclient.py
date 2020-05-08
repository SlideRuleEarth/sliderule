# python

import sys
import json
import requests

server_url = 'http://127.0.0.1:9081'

#
#  ECHO
#
def echo (parm):
    url = '%s/echo' % server_url
    return requests.post(url, data=parm)


#
#  SOURCE
#
def source (api, parm):
    url = '%s/source/%s' % (server_url, api)
    return requests.post(url, data=parm)

#
#  ENGINE
#
def engine (api, parm):
    url = '%s/engine/%s' % (server_url, api)
    return requests.post(url, data=parm)


###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line

    if len(sys.argv) > 1:
        server_url = sys.argv[1]

    # Test echo api

    d = echo('{ "hello" : "world" }').json()

    if d["hello"] == "world":
        print("Passed echo test")
    else:
        print("Failed echo test")

    # Test time api

    rqst_dict = {
        "time": "NOW",
        "input": "NOW",
        "output": "GPS"
    }

    d = source("time", json.dumps(rqst_dict)).json()

    now = d["time"] - (d["time"] % 1000) # gmt is in resolution of seconds, not milliseconds
    rqst_dict["time"] = d["time"]
    rqst_dict["input"] = "GPS"
    rqst_dict["output"] = "GMT"

    d = source("time", json.dumps(rqst_dict)).json()

    rqst_dict["time"] = d["time"]
    rqst_dict["input"] = "GMT"
    rqst_dict["output"] = "GPS"

    d = source("time", json.dumps(rqst_dict)).json()

    again = d["time"]

    if now == again:
        print("Passed time test")
    else:
        print("Failed time test")

