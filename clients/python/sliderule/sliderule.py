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
import numpy
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
server_lock = threading.Condition()
server_table = {}

max_errors_per_server = 3
max_pending_per_server = 3
max_retries_per_request = 5

verbose = False

request_timeout = (10, 60) # (connection, read) in seconds

logger = logging.getLogger(__name__)

recdef_table = {}

gps_epoch = datetime(1980, 1, 6)
tai_epoch = datetime(1970, 1, 1, 0, 0, 10)

eventformats = {
    "TEXT":     0,
    "JSON":     1
}

eventlogger = {
    0: logger.debug,
    1: logger.info,
    2: logger.warning,
    3: logger.error,
    4: logger.critical
}

handleexcept = {
    0: {"name": "ERROR",                    "fatal": True,  "expected": False },
    1: {"name": "TIMEOUT",                  "fatal": False, "expected": True },
    2: {"name": "RESOURCE_DOES_NOT_EXIST",  "fatal": True,  "expected": True },
    3: {"name": "EMPTY_SUBSET",             "fatal": True,  "expected": True }
}

datatypes = {
    "TEXT":     0,
    "REAL":     1,
    "INTEGER":  2,
    "DYNAMIC":  3
}

basictypes = {
    "INT8":     { "fmt": 'b', "size": 1, "nptype": numpy.int8 },
    "INT16":    { "fmt": 'h', "size": 2, "nptype": numpy.int16 },
    "INT32":    { "fmt": 'i', "size": 4, "nptype": numpy.int32 },
    "INT64":    { "fmt": 'q', "size": 8, "nptype": numpy.int64 },
    "UINT8":    { "fmt": 'B', "size": 1, "nptype": numpy.uint8 },
    "UINT16":   { "fmt": 'H', "size": 2, "nptype": numpy.uint16 },
    "UINT32":   { "fmt": 'I', "size": 4, "nptype": numpy.uint32 },
    "UINT64":   { "fmt": 'Q', "size": 8, "nptype": numpy.uint64 },
    "BITFIELD": { "fmt": 'x', "size": 0, "nptype": numpy.byte },    # unsupported
    "FLOAT":    { "fmt": 'f', "size": 4, "nptype": numpy.single },
    "DOUBLE":   { "fmt": 'd', "size": 8, "nptype": numpy.double },
    "TIME8":    { "fmt": 'Q', "size": 8, "nptype": numpy.byte },
    "STRING":   { "fmt": 's', "size": 1, "nptype": numpy.byte }
}

codedtype2str = {
    0:  "INT8",
    1:  "INT16",
    2:  "INT32",
    3:  "INT64",
    4:  "UINT8",
    5:  "UINT16",
    6:  "UINT32",
    7:  "UINT64",
    8:  "BITFIELD",
    9:  "FLOAT",
    10: "DOUBLE",
    11: "TIME8",
    12: "STRING"
}

###############################################################################
# CLASSES
###############################################################################

class TransientError(Exception):
    """Processing exception that can be retried"""
    pass

###############################################################################
# UTILITIES
###############################################################################

#
#  get the ip address of an available sliderule server
#
def __getserv(stream):
    global server_table
    with server_lock:
        try:
            server_available = False
            while not server_available:
                serv = min(server_table.items(), key=lambda x:x[1]["pending"])[0]
                if stream:
                    if server_table[serv]["pending"] < max_pending_per_server:
                        server_table[serv]["pending"] += 1
                        server_available = True
                    else:
                        if not server_lock.wait(1.0):
                            logger.debug("Timeout occurred waiting for thread to be notified")
                else:
                    server_available = True
        except:
            raise RuntimeError('No available urls')
    return serv

#
#  __errserv
#
def __errserv(serv, stream):
    global server_table, max_errors_per_server
    with server_lock:
        try:
            if stream:
                server_table[serv]["pending"] -= 1
            server_table[serv]["errors"] += 1
            logger.warning(serv + " encountered consecutive error " + str(server_table[serv]["errors"]))
            if server_table[serv]["errors"] > max_errors_per_server:
                logger.critical("Removing " + serv + " from list of available servers due to too many consecutive errors")
                server_table.pop(serv, None)
        except Exception as e:
            logger.debug(serv + " already removed from table")
        finally:
            if stream:
                server_lock.notify()

#
#  __clrserv
#
def __clrserv(serv, stream):
    global server_table
    with server_lock:
        try:
            if stream:
                server_table[serv]["pending"] -= 1
            server_table[serv]["errors"] = 0
        except Exception as e:
            pass
        finally:
            server_lock.notify()

#
#  __populate
#
def __populate(rectype):
    global recdef_table
    if rectype not in recdef_table:
        recdef_table[rectype] = source("definition", {"rectype" : rectype})
    return recdef_table[rectype]

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

    # get/populate record definition #
    recdef = __populate(rectype)

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
            subrecdef = __populate(ftype)

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
        eventlogger[rec['level']]('%s' % (rec["attr"]))

#
#  __raiseexceptrec
#
def __raiseexceptrec(rec):
    rc = rec["code"]
    if rc in handleexcept:
        if verbose:
            logger.info("%s exception <%d>: %s", handleexcept[rc]["name"], rc, rec["text"])
        if not handleexcept[rc]["expected"]:
            logger.critical("Unexpected error: %s", handleexcept[rc]["name"])
        if not handleexcept[rc]["fatal"]:
            raise TransientError()

###############################################################################
# APIs
###############################################################################

#
#  SOURCE
#
def source (api, parm={}, stream=False, callbacks={'eventrec': __logeventrec, 'exceptrec': __raiseexceptrec}):
    '''
    Perform API call to SlideRule service

    Parameters
    ----------
        api:        str
                    name of the SlideRule endpoint
        parm:       dict
                    dictionary of request parameters
        stream:     bool
                    whether the request is a **normal** service or a **stream** service (see `De-serialization <./SlideRule.html#de-serialization>`_ for more details)
        callbacks:  dict
                    record type callbacks (advanced use)

    Returns
    -------
    bytearray
        response data

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_url("icesat2sliderule.org")
        >>> rqst = {
        ...     "time": "NOW",
        ...     "input": "NOW",
        ...     "output": "GPS"
        ... }
        >>> rsps = sliderule.source("time", rqst)
        >>> print(rsps)
        {'time': 1300556199523.0, 'format': 'GPS'}
    '''
    rqst = json.dumps(parm)
    complete = False
    retries = max_retries_per_request
    rsps = []
    while (not complete) and (retries > 0):
        retries -= 1
        serv = __getserv(stream) # it throws a RuntimeError that must be caught by calling function
        try:
            url  = '%s/source/%s' % (serv, api)
            if not stream:
                rsps = requests.get(url, data=rqst, timeout=request_timeout).json()
            else:
                data = requests.post(url, data=rqst, timeout=request_timeout, stream=True)
                data.raise_for_status()
                rsps = __parse(data, callbacks)
            __clrserv(serv, stream)
            complete = True
        except requests.ConnectionError as e:
            logger.error("Failed to connect to endpoint {} ... retrying request".format(url))
            __errserv(serv, stream)
        except requests.Timeout as e:
            logger.error("Timed-out waiting for response from endpoint {} ... retrying request".format(url))
            __errserv(serv, stream)
        except requests.exceptions.ChunkedEncodingError as e:
            logger.error("Unexpected termination of response from endpoint {} ... retrying request".format(url))
            __errserv(serv, stream)
        except requests.HTTPError as e:
            if e.response.status_code == 503:
                logger.error("Server experiencing heavy load, stalling on request to {} ... will retry".format(url))
                __clrserv(serv, stream)
            else:
                logger.error("Invalid HTTP response from endpoint {} ... retrying request".format(url))
                __errserv(serv, stream)
        except TransientError as e:
            logger.warning("Recoverable error occurred at {} ... retrying request".format(url))
            __clrserv(serv, stream)
        except:
            __errserv(serv, stream)
            raise
    return rsps

#
#  SET_URL
#
def set_url (urls):
    '''
    Configure sliderule package with URL of service

    Parameters
    ----------
        urls:   str
                IP address or hostname of SlideRule service (note, there is a special case where the url is provided as a list of strings
                instead of just a string; when a list is provided, the client hardcodes the set of servers that are used to process requests
                to the exact set provided; this is used for testing and for local installations and can be ignored by most users)

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_url("service.my-sliderule-server.org")
    '''
    global server_table, service_url
    with server_lock:
        service_url = None
        server_table = {}
        if type(urls) == list: # hardcoded list of sliderule server IP addresses
            for serv in urls:
                server_table["http://" + serv] = {"pending": 0, "errors": 0}
        elif type(urls) == str: # IP address of sliderule's service discovery
            service_url = "http://" + urls + "/discovery/"
        else:
            raise TypeError('expected ip address or hostname as a string or list of strings')
    # then update server table
    update_available_servers()

#
#  UPDATE_AVAIABLE_SERVERS
#
def update_available_servers ():
    '''
    Causes the SlideRule Python client to refresh the list of available processing nodes. This is useful when performing large processing
    requests where there is time for auto-scaling to change the number of nodes running.

    This function does nothing if the client has been initialized with a hardcoded list of servers.

    Returns
    -------
    int
        the number of available processing nodes

    Examples
    --------
        >>> import sliderule
        >>> sliderule.update_available_servers()
    '''
    global server_table, service_url
    with server_lock:
        if service_url != None:
            server_table = {}
            response = requests.get(service_url, data='{"service":"sliderule"}', timeout=request_timeout).json()
            for entry in response['members']:
                server_table["http://" + entry] = {"pending": 0, "errors": 0}
        num_servers = len(server_table)
        max_workers = num_servers * max_pending_per_server
    return (num_servers, max_workers)

#
#  SET_VERBOSE
#
def set_verbose (enable):
    '''
    Configure sliderule package for verbose logging

    Parameters
    ----------
        enable:     bool
                    whether or not user level log messages received from SlideRule generate a Python log message

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_verbose(True)

        The default behavior of Python log messages is for them to be displayed to standard output.
        If you want more control over the behavior of the log messages being display, create and configure a Python log handler as shown below:

        >>> # import packages
        >>> import logging
        >>> from sliderule import sliderule
        >>> # Configure Logging
        >>> sliderule_logger = logging.getLogger("sliderule.sliderule")
        >>> sliderule_logger.setLevel(logging.INFO)
        >>> # Create Console Output
        >>> ch = logging.StreamHandler()
        >>> ch.setLevel(logging.INFO)
        >>> sliderule_logger.addHandler(ch)
    '''
    global verbose
    verbose = (enable == True)

#
#  SET_MAX_ERRORS
#
def set_max_errors (max_errors):
    '''
    Configure sliderule package's maximum number of errors per node setting.  When the client makes a request to a processing node,
    if there is an error, it will retry the request to a different processing node (if available), but will keep the original processing
    node in the list of available nodes and increment the number of errors associated with it.  But if a processing node accumulates up
    to the **max_errors** number of errors, then the node is removed from the list of available nodes and will not be used in future
    processing requests.

    A call to ``update_available_servers`` or ``set_url`` is needed to restore a removed node to the list of available servers.

    Parameters
    ----------
        max_errors:     int
                        sets the maximum number of errors per node

    Raises
    ------
    TypeError
        max_errors must be a positive integer

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_max_errors(3)
    '''
    global max_errors_per_server
    if max_errors > 0:
        max_errors_per_server = max_errors
    else:
        raise TypeError('max errors must be greater than zero')

#
#  SET_MAX_PENDING
#
def set_max_pending (max_pending):
    '''
    Configure sliderule package's maximum number of pending requests per node setting.  When the client makes a request, it first finds the processing
    node with the fewest requests in progress (i.e. pending), and sends the request to that node.  If the node with the fewest pending requests has
    `max_pending` requests, it will wait until a processing node becomes available with less than `max_pending` requests.

    Parameters
    ----------
        max_pending:    int
                        sets the maximum number of pending requests per node

    Raises
    ------
    TypeError
        max_pending must be a positive integer

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_max_pending(1)
    '''
    global max_pending_per_server
    if max_pending > 0:
        max_pending_per_server = max_pending
    else:
        raise TypeError('max pending must be greater than zero')

#
# SET_REQUEST_TIMEOUT
#
def set_rqst_timeout (timeout):
    '''
    Sets the TCP/IP connection and reading timeouts for future requests made to sliderule servers.
    Setting it lower means the client will failover more quickly, but may generate false positives if a processing request stalls or takes a long time returning data.
    Setting it higher means the client will wait longer before designating it a failed request which in the presence of a persistent failure means it will take longer for the client to remove the node from its available servers list.

    Parameters
    ----------
        timeout:    tuple
                    (<connection timeout in seconds>, <read timeout in seconds>)

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_rqst_timeout((10, 60))
    '''
    global request_timeout
    if type(timeout) == tuple:
        request_timeout = timeout
    else:
        raise TypeError('timeout must be a tuple (<connection timeout>, <read timeout>)')

#
# GPS2UTC
#
def gps2utc (gps_time, as_str=True, epoch=gps_epoch):
    '''
    Convert a GPS based time returned from SlideRule into a UTC time.

    Parameters
    ----------
        gps_time:   int
                    number of seconds since GPS epoch (January 6, 1980)
        as_str:     bool
                    if True, returns the time as a string; if False, returns the time as datatime object
        epoch:      datetime
                    the epoch used in the conversion, defaults to GPS epoch (Jan 6, 1980)

    Returns
    -------
    datetime
        UTC time (i.e. GMT, or Zulu time)

    Examples
    --------
        >>> import sliderule
        >>> sliderule.gps2utc(1235331234)
        '2019-02-27 19:34:03'
    '''
    gps_time = epoch + timedelta(seconds=gps_time)
    tai_time = gps_time + timedelta(seconds=19)
    tai_timestamp = (tai_time - tai_epoch).total_seconds()
    utc_timestamp = datetime.utcfromtimestamp(tai_timestamp)
    if as_str:
        return str(utc_timestamp)
    else:
        return utc_timestamp

#
# GET DEFINITION
#
def get_definition (rectype, fieldname):
    '''
    Get the underlying format specification of a field in a return record.

    Parameters
    ----------
    rectype:    str
                the name of the type of the record (i.e. "atl03rec")
    fieldname:  str
                the name of the record field (i.e. "cycle")

    Returns
    -------
    dict
        description of each field; see the `sliderule.basictypes` variable for different field types

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_url("icesat2sliderule.org")
        >>> sliderule.get_definition("atl03rec", "cycle")
        {'fmt': 'H', 'size': 2, 'nptype': <class 'numpy.uint16'>}
    '''
    recdef = __populate(rectype)
    if fieldname in recdef and recdef[fieldname]["type"] in basictypes:
        return basictypes[recdef[fieldname]["type"]]
    else:
        return {}
