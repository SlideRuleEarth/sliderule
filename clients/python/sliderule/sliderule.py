# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import requests
import json
import struct
import ctypes
import logging
import threading
from datetime import datetime, timedelta

###############################################################################
# GLOBALS
###############################################################################

service_url = None

server_lock = threading.Lock()
server_table = {}
server_index = 0
server_max_errors = 3

verbose = False

logger = logging.getLogger(__name__)

recdef_lock = threading.Lock()
recdef_table = {}

gps_epoch = datetime(1980, 1, 6)
tai_epoch = datetime(1970, 1, 1, 0, 0, 10)

eventformats = {
    "TEXT":     0,
    "JSON":     1
}

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

#
#  set verbose
#
def __setverb(enable):
    global verbose
    verbose = (enable == True)

#
#  set maximum server errors allowed
#
def __setmaxerr(max_errors):
    global server_max_errors
    if max_errors > 0:
        server_max_errors = max_errors
    else:
        raise TypeError('max errors must be greater than zero')

#
#  set server urls
#   - you either pass in a single ip address or hostname which is used for service discovery
#   - OR you pass in a list of ip address or hostnames which are treated as a fixed list of servers
#
def __setserv(servs):
    global server_table, server_index, service_url
    with server_lock:
        service_url = None
        server_index = 0
        server_table = {}
        if type(servs) == list: # hardcoded list of sliderule server IP addresses
            for serv in servs:
                server_url = "http://" + serv + ":9081"
                server_table[server_url] = 0
        elif type(servs) == str: # IP address of sliderule's service discovery
            service_url = "http://" + servs + ":8500/v1/catalog/service/srds?passing"
        else:
            raise TypeError('expected ip address or hostname as a string or list of strings')
    # then update server table
    __upserv()

#
# update server urls
#
def __upserv():
    global server_table, server_index, service_url
    with server_lock:
        if service_url != None:
            server_index = 0
            server_table = {}
            services = requests.get(service_url).json()
            for entry in services:
                server_url = "http://" + entry["Address"] + ":" + str(entry["ServicePort"])
                server_table[server_url] = 0
    return len(server_table)

#
#  get the ip address of an available sliderule server
#
def __getserv():
    global server_table, server_index
    serv = ""
    with server_lock:
        server_list = list(server_table.keys())
        num_server_tables = len(server_list)
        if num_server_tables == 0:
            raise RuntimeError('No available urls')
        server_index = (server_index + 1) % num_server_tables
        serv = server_list[server_index]
    return serv

#
#  __errserv
#
def __errserv(serv):
    global server_table, server_max_errors
    with server_lock:
        try:
            server_table[serv] += 1
            logger.critical(serv + " encountered consecutive error " + str(server_table[serv]))
            if server_table[serv] > server_max_errors:
                server_table.pop(serv, None)
        except Exception as e:
            logger.critical(serv + " already removed from table")

#
#  __clrserv
#
def __clrserv(serv):
    global server_table
    with server_lock:
        server_table[serv] = 0

#
#  __populate
#
def __populate(rectype):
    global recdef_table
    with recdef_lock:
        need_to_populate = rectype not in recdef_table
    if need_to_populate:
        recdef = source("definition", {"rectype" : rectype})
        with recdef_lock:
            recdef_table[rectype] = recdef

#
#  __decode
#
def __decode(rectype, rawdata):
    """
    rectype: record type supplied in response (string)
    rawdata: payload supplied in response (byte array)
    """
    global recdef_table

    # initialize record
    rec = { "__rectype": rectype }

    # populate record definition (if needed) #
    __populate(rectype)

    # get record definition
    with recdef_lock:
        recdef = recdef_table[rectype]

    # iterate through each field in definition
    for fieldname in recdef.keys():

        # double underline (__) as prefix indicates meta data
        if fieldname.find("__") == 0:
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

            # check if array
            is_array = not (elems == 1)

            # get number of elements
            if elems <= 0:
                elems = int((len(rawdata) - offset) / basictypes[ftype]["size"])

            # build format string
            fmt = endian + str(elems) + basictypes[ftype]["fmt"]

            # parse data
            value = struct.unpack_from(fmt, rawdata, offset)

            # set field
            if ftype == "STRING":
                rec[fieldname] = ctypes.create_string_buffer(value[0]).value.decode('ascii')
            elif is_array:
                rec[fieldname] = value
            else:
                rec[fieldname] = value[0]

        # decode user type
        else:

            # populate record definition (if needed) #
            __populate(ftype)

            # decode record
            subrec = {}
            with recdef_lock:
                subrecdef = recdef_table[ftype]

            # check if array
            is_array = not (elems == 1)

            # get number of elements
            if elems <= 0:
                elems = int((len(rawdata) - offset) / subrecdef["__datasize"])

            # return parsed data
            if is_array:
                rec[fieldname] = []
                for e in range(elems):
                    rec[fieldname].append(__decode(ftype, rawdata[offset:]))
                    offset += subrecdef["__datasize"]
            else:
                rec[fieldname] = __decode(ftype, rawdata[offset:])

    # return record #
    return rec

#
#  __parse
#
def __parse(stream, callbacks):
    """
    stream: request response stream
    """
    recs = []

    rec_size_size = 4
    rec_size_index = 0
    rec_size_rsps = []

    rec_size = 0
    rec_index = 0
    rec_rsps = []

    for line in stream.iter_content(None):

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
                    rec_size = struct.unpack('<i', raw)[0]
                    rec_size_rsps.clear()
                i += bytes_to_append

            # Parse Record
            elif(rec_size > 0):
                bytes_available = len(line) - i
                bytes_remaining = rec_size - rec_index
                bytes_to_append = min(bytes_available, bytes_remaining)
                rec_rsps.append(line[i:i+bytes_to_append])
                rec_index += bytes_to_append
                if(rec_index >= rec_size):
                    # Decode Record
                    rawbits = b''.join(rec_rsps)
                    rectype = ctypes.create_string_buffer(rawbits).value.decode('ascii')
                    rawdata = rawbits[len(rectype) + 1:]
                    rec     = __decode(rectype, rawdata)
                    if callbacks != None and rectype in callbacks:
                        # Execute Call-Back on Record
                        callbacks[rectype](rec)
                    else:
                        # Append Record
                        recs.append(rec)
                    # Reset Record Parsing
                    rec_rsps.clear()
                    rec_size_index = 0
                    rec_size = 0
                    rec_index = 0
                i += bytes_to_append

    return recs

#
#  __logeventrec
#
def __logeventrec(rec):
    if verbose:
        logger.critical('%s' % (rec["attr"]))

###############################################################################
# APIs
###############################################################################

#
#  SOURCE
#
def source (api, parm={}, stream=False, callbacks={'eventrec': __logeventrec}):
    rqst = json.dumps(parm)
    complete = False
    while not complete:
        serv = __getserv()
        try:
            url  = '%s/source/%s' % (serv, api)
            if not stream:
                rsps = requests.get(url, data=rqst).json()
            else:
                data = requests.post(url, data=rqst, stream=True)
                rsps = __parse(data, callbacks)
            __clrserv(serv)
            complete = True
        except Exception as e:
            logger.critical(e)
            __errserv(serv)
    return rsps

#
#  SET_URL
#
def set_url (urls):
    __setserv(urls)

#
#  UPDATE_AVAIABLE_SERVERS
#
def update_available_servers ():
    return __upserv()

#
#  SET_VERBOSE
#
def set_verbose (enable):
    __setverb(enable)

#
#  SET_MAX_ERRORS
#
def set_max_errors (max_errors):
    __setmaxerr(max_errors)

#
# GPS2UTC
#
def gps2utc (gps_time, as_str=True):
    gps_time = gps_epoch + timedelta(seconds=gps_time)
    tai_time = gps_time + timedelta(seconds=19)
    tai_timestamp = (tai_time - tai_epoch).total_seconds()
    utc_timestamp = datetime.utcfromtimestamp(tai_timestamp)
    if as_str:
        return str(utc_timestamp)
    else:
        return utc_timestamp
