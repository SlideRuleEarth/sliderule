# python

import sys
import requests
import json
import struct
import binascii # used for binascii.hexlify(<data>)
import ctypes
import array

import pandas as pd
import numpy as np

###############################################################################
# GLOBALS
###############################################################################

server_url = 'http://127.0.0.1:9081'

recdef_tbl = {}

datatypes = {
    "TEXT":     0,
    "REAL":     1,
    "INTEGER":  2,
    "DYNAMIC":  3
}

basictypes = {
    "INT8":     { "fmt": 'b', "size": 1 },
    "INT16":    { "fmt": 'h', "size": 2 },
    "INT32":    { "fmt": 'i', "size": 4 },
    "INT64":    { "fmt": 'q', "size": 8 },
    "UINT8":    { "fmt": 'B', "size": 1 },
    "UINT16":   { "fmt": 'H', "size": 2 },
    "UINT32":   { "fmt": 'I', "size": 4 },
    "UINT64":   { "fmt": 'Q', "size": 8 },
    "BITFIELD": { "fmt": 'x', "size": 0 },    # unsupported
    "FLOAT":    { "fmt": 'f', "size": 4 },
    "DOUBLE":   { "fmt": 'd', "size": 8 },
    "TIME8":    { "fmt": 'Q', "size": 8 },
    "STRING":   { "fmt": 's', "size": 1 }
}

###############################################################################
# UTILITIES
###############################################################################

def __decode(rectype, rawdata):
    """
    rectype: record type supplied in response (string)
    rawdata: payload supplied in response (byte array)
    """

    # initialize record
    rec = { "@rectype": rectype }

    # get record definition
    if rectype in recdef_tbl:
        recdef = recdef_tbl[rectype]
    else:
        return rec

    # iterate through each field in definition
    for fieldname in recdef.keys():

        # @ indicates meta data
        if "@" in fieldname:
            continue

        # get field properties
        field = recdef[fieldname]
        ftype = field["type"]
        offset = int(field["offset"] / 8)
        elems = field["elements"]
        flags = field["flags"]

        # do not process pointers
        if "PTR" in flags:
            continue

        # get endianess
        if "LE" in flags:
            endian = '<'
        else:
            endian = '>'

        # decode basic type
        if ftype in basictypes:

            # get number of elements
            if elems <= 0:
                elems = int((len(rawdata) - offset) / basictypes[ftype]["size"])

            # build format string
            fmt = endian + str(elems) + basictypes[ftype]["fmt"]

            # return parsed data
            if elems == 1:
                rec[fieldname] = struct.unpack_from(fmt, rawdata, offset)[0]
            else:
                rec[fieldname] = struct.unpack_from(fmt, rawdata, offset)

        # decode user type
        elif ftype in recdef_tbl:

            subrec = {}
            subrecdef = recdef_tbl[ftype]

            # get number of elements
            if elems <= 0:
                elems = int((len(rawdata) - offset) / subrecdef["@datasize"])

            # return parsed data
            if elems == 1:
                rec[fieldname] = __decode(ftype, rawdata[offset:])
            else:
                rec[fieldname] = []
                for e in range(elems):
                    offset += subrecdef["@datasize"]
                    rec[fieldname].append(__decode(ftype, rawdata[offset:]))

    # return record #
    return rec

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
    rqst   = json.dumps(parm)
    url    = '%s/engine/%s' % (server_url, api)
    stream = requests.post(url, data=rqst, stream=True)

    # Read and Parse Stream #
    rsps_recs = []

    rec_size_size = 4
    rec_size_index = 0
    rec_size_rsps = []

    rec_size = 0
    rec_index = 0
    rec_rsps = []

    for line in stream.iter_content(0x10000):

        i = 0
        while i < len(line):

            # Parse Record Size
            if(rec_size_index < rec_size_size):
                bytes_available = len(line)  - i
                bytes_remaining = rec_size_size - rec_size_index
                bytes_to_append = min(bytes_available, bytes_remaining)
                rec_size_rsps.append(line[i:i+bytes_to_append])
                rec_size_index += bytes_to_append
                if(rec_size_index >= rec_size_size):
                    raw = b''.join(rec_size_rsps)
                    rec_size_rsps.clear()
                    rec_size = struct.unpack('i', raw)[0]
                i += bytes_to_append

            # Parse Record
            elif(rec_size > 0):
                bytes_available = len(line) - i
                bytes_remaining = rec_size - rec_index
                bytes_to_append = min(bytes_available, bytes_remaining)
                rec_rsps.append(line[i:i+bytes_to_append])
                rec_index += bytes_to_append
                if(rec_index >= rec_size):
                    raw = b''.join(rec_rsps)
                    rsps_recs.append(raw)
                    rec_rsps.clear()
                    rec_size_index = 0
                    rec_size = 0
                    rec_index = 0
                i += bytes_to_append

    # Build Response #
    rsps = []
    for rawrec in rsps_recs:
        rectype = ctypes.create_string_buffer(rawrec).value.decode('ascii')
        rawdata = rawrec[len(rectype) + 1:]
        rec     = __decode(rectype, rawdata)
        rsps.append(rec)

    # Return Response #
    return rsps

#
#  POPULATE
#
def populate(rectype):
    global recdef_tbl
    recdef_tbl[rectype] = source("definition", {"rectype" : rectype})

#
#  SET_URL
#
def set_url(new_url):
    global server_url
    server_url = new_url

#
#  GET_VALUES
#
def get_values(data, dtype, size):
    """
    data:   tuple of bytes
    dtype:  element of datatypes
    size:   bytes in data
    """

    datatype2nptype = {
        datatypes["TEXT"]:      np.byte,
        datatypes["REAL"]:      np.double,
        datatypes["INTEGER"]:   np.int32,
        datatypes["DYNAMIC"]:   np.byte
    }

    raw = bytes(data)
    datatype = datatype2nptype[dtype]
    datasize = int(size / np.dtype(datatype).itemsize)
    slicesize = datasize * np.dtype(datatype).itemsize # truncates partial bytes
    values = np.frombuffer(raw[:slicesize], dtype=datatype, count=datasize)

    return values
