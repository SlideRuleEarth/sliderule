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

datatype2nptype = {
    datatypes["TEXT"]:      np.byte,
    datatypes["REAL"]:      np.double,
    datatypes["INTEGER"]:   np.int32,
    datatypes["DYNAMIC"]:   np.byte
}

type2fmt = {
    "INT8":     'b',
    "INT16":    'h',
    "INT32":    'i',
    "INT64":    'q',
    "UINT8":    'B',
    "UINT16":   'H',
    "UINT32":   'I',
    "UINT64":   'Q',
    "BITFIELD": 'x',    # unsupported
    "FLOAT":    'f',
    "DOUBLE":   'd',
    "TIME8":    'Q',
    "STRING":   's'
}

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
    #
    #   Notes:  1. TODO: Only one level of subrecords currently supported
    #           2. TODO: Endianness set not always set per field
    #           3. TODO: break this out into helper functions with recursion
    #           4. TODO: general optimization... if subrecord has no arrays, could prebuild fmt string
    rsps = []
    for rawrec in rsps_recs:
        rec = {}
        rectype = ctypes.create_string_buffer(rawrec).value.decode('ascii')
        rawdata = rawrec[len(rectype) + 1:]
        rec["@rectype"] = rectype
        if rectype in recdef_tbl:
            recdef = recdef_tbl[rectype]
            for field in recdef.keys():
                if "@" not in field: # @ indicates meta data
                    flags = recdef[field]["flags"]
                    if "LE" in flags:
                        endian = '<'
                    else:
                        endian = '>'
                    if "PTR" not in flags:
                        ftype = recdef[field]["type"]
                        elems = recdef[field]["elements"]
                        offset = int(recdef[field]["offset"] / 8)
                        if elems <= 0:
                            elems = len(rawdata) - offset
                        if ftype in type2fmt:
                            fmt = endian + str(elems) + type2fmt[ftype]
                            if elems == 1:
                                val = struct.unpack_from(fmt, rawdata, offset)[0]
                            else:
                                val = struct.unpack_from(fmt, rawdata, offset)
                            rec[field] = val
                        elif ftype in recdef_tbl:
                            subrec = {}
                            subrecdef = recdef_tbl[ftype]
                            for e in range(elems):
                                for subfield in subrecdef.keys():
                                    if "@" not in subfield:
                                        subelems = subrecdef[subfield]["elements"]
                                        suboffset = int(subrecdef[subfield]["offset"] / 8)
                                        subftype = subrecdef[subfield]["type"]
                                        fmt = endian + str(subelems) + type2fmt[subftype]
                                        offset = suboffset + int(e * subrecdef["@datasize"])
                                        if subelems == 1:
                                            val = struct.unpack_from(fmt, rawdata, offset)[0]
                                        else:
                                            val = struct.unpack_from(fmt, rawdata, offset)
                                        subrec[subfield] = val
                            rec[field] = subrec
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
    raw = bytes(data)
    datatype = datatype2nptype[dtype]
    datasize = int(size / np.dtype(datatype).itemsize)
    slicesize = datasize * np.dtype(datatype).itemsize # truncates partial bytes
    values = np.frombuffer(raw[:slicesize], dtype=datatype, count=datasize)

    return values
