# python

import sys
import requests
import json
import struct
import binascii

import pandas as pd
import numpy as np

###############################################################################
# GLOBALS
###############################################################################

server_url = 'http://127.0.0.1:9081'
recdef_tbl = {}

###############################################################################
# APIs
###############################################################################

#
#  ECHO
#
def echo (parm):
    rqst = json.dumps(parm)
    url  = '%s/echo' % server_url
    rsps = requests.post(url, data=rqst).json()
    return rsps

#
#  SOURCE
#
def source (api, parm):
    rqst = json.dumps(parm)
    url  = '%s/source/%s' % (server_url, api)
    rsps = requests.post(url, data=rqst).json()
    return rsps

#
#  ENGINE
#
def engine (api, parm):
    rqst = json.dumps(parm)
    url  = '%s/engine/%s' % (server_url, api)
    rsps = requests.post(url, data=rqst, stream=True)

    # Read and Parse Response #
    rsps_recs = []

    rec_size_size = 4
    rec_size_index = 0
    rec_size_rsps = []

    rec_size = 0
    rec_index = 0
    rec_rsps = []

    for line in rsps.iter_content(0x10000):
        i = 0
        while i < len(line):

            # Parse Record Size #
            if(rec_size_index < rec_size_size):
                bytes_available = len(line) - i
                bytes_remaining = rec_size_size - rec_size_index
                bytes_to_append = min(bytes_available, bytes_remaining)
                rec_size_rsps.append(line[i:i+bytes_to_append])
                rec_size_index += bytes_to_append
                if(rec_size_index >= rec_size_size):
                    raw = b''.join(rec_size_rsps)
                    rec_size_rsps.clear()
                    rec_size = struct.unpack('i', raw)[0]
                i += bytes_to_append

            # Parse Record #
            elif(rec_size > 0):
                bytes_available = len(line) - i
                bytes_remaining = rec_size - rec_index
                bytes_to_append = min(bytes_available, bytes_remaining)
                rec_rsps.append(line[i:i+bytes_to_append])
                rec_index += bytes_to_append
                if(rec_index >= rec_size):
                    raw = b''.join(rec_rsps)
                    rsps_recs.append(raw)
                    rec_size_index = 0
                    rec_size = 0
                    rec_index = 0
                i += bytes_to_append

    # Build DataFrame
    dataset = "test"
    datatype = np.int32
    datasize = int(len(rsps_recs[0]) / np.dtype(datatype).itemsize)
    values = np.frombuffer(raw, dtype=datatype, count=datasize)
    df = pd.DataFrame(data=values, index=[i for i in range(datasize)], columns=[dataset])

    return df

###############################################################################
# ON IMPORT
###############################################################################

recdef_tbl["atl03rec"] = source("definition", {"rectype" : "atl03rec"})
recdef_tbl["h5dataset"] = source("definition", {"rectype" : "h5dataset"})
recdef_tbl["atl03rec.photons"] = source("definition", {"rectype" : "atl03rec.photons"})

###############################################################################
# MAIN
###############################################################################

if __name__ == '__main__':

    # Override server URL from command line

    if len(sys.argv) > 1:
        server_url = sys.argv[1]

    # Request Dataset

    parms = {
        "filename": "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5",
        "track": 1,
        "stages": ["LSF"],
        "parms": {
            "cnf": 4,
            "ats": 20.0,
            "cnt": 10,
            "len": 40.0,
            "res": 20.0
        }
    }

#    df = engine("atl06", parms)

    parms = {
        "filename": "/data/ATLAS/ATL03_20200304065203_10470605_003_01.h5",
        "dataset": "/gt1r/geolocation/segment_ph_cnt",
        "id": 0
    }

#    df = engine("h5", parms)

#    print(df.head())

    print(recdef_tbl)
