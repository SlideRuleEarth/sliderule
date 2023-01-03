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

import os
import netrc
import requests
import json
import struct
import ctypes
import time
import logging
import numpy
from datetime import datetime, timedelta
from sliderule import version

###############################################################################
# GLOBALS
###############################################################################

PUBLIC_URL = "slideruleearth.io"
PUBLIC_ORG = "sliderule"

service_url = PUBLIC_URL
service_org = PUBLIC_ORG

session = requests.Session()
session.trust_env = False

ps_refresh_token = None
ps_access_token = None
ps_token_exp = None

verbose = False

request_timeout = (10, 60) # (connection, read) in seconds

logger = logging.getLogger(__name__)

recdef_table = {}

arrow_file_table = {}

profiles = {}

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

datatypes = {
    "TEXT":     0,
    "REAL":     1,
    "INTEGER":  2,
    "DYNAMIC":  3
}

basictypes = {
    "INT8":     { "fmt": 'b', "size": 1, "nptype": numpy.int8   },
    "INT16":    { "fmt": 'h', "size": 2, "nptype": numpy.int16  },
    "INT32":    { "fmt": 'i', "size": 4, "nptype": numpy.int32  },
    "INT64":    { "fmt": 'q', "size": 8, "nptype": numpy.int64  },
    "UINT8":    { "fmt": 'B', "size": 1, "nptype": numpy.uint8  },
    "UINT16":   { "fmt": 'H', "size": 2, "nptype": numpy.uint16 },
    "UINT32":   { "fmt": 'I', "size": 4, "nptype": numpy.uint32 },
    "UINT64":   { "fmt": 'Q', "size": 8, "nptype": numpy.uint64 },
    "BITFIELD": { "fmt": 'x', "size": 0, "nptype": numpy.byte   },  # unsupported
    "FLOAT":    { "fmt": 'f', "size": 4, "nptype": numpy.single },
    "DOUBLE":   { "fmt": 'd', "size": 8, "nptype": numpy.double },
    "TIME8":    { "fmt": 'Q', "size": 8, "nptype": numpy.byte   },
    "STRING":   { "fmt": 's', "size": 1, "nptype": numpy.byte   }
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
# CLIENT EXCEPTIONS
###############################################################################

class FatalError(RuntimeError):
    pass

class TransientError(RuntimeError):
    pass

###############################################################################
# UTILITIES
###############################################################################

#
#  __populate
#
def __populate(rectype):
    global recdef_table
    if rectype not in recdef_table:
        recdef_table[rectype] = source("definition", {"rectype" : rectype})
    return recdef_table[rectype]

#
#  __parse_json
#
def __parse_json(data):
    """
    data: request response
    """
    lines = []
    for line in data.iter_content(None):
        lines.append(line)
    response = b''.join(lines)
    return json.loads(response)

#
#  __decode_native
#
def __decode_native(rectype, rawdata):
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
                    rec[fieldname].append(__decode_native(ftype, rawdata[offset:]))
                    offset += subrecdef["__datasize"]
            else:
                rec[fieldname] = __decode_native(ftype, rawdata[offset:])

    # return record #
    return rec

#
#  __parse_native
#
def __parse_native(data, callbacks):
    """
    data: request response
    """
    recs = []

    rec_hdr_size = 8
    rec_size_index = 0
    rec_size_rsps = []

    rec_size = 0
    rec_index = 0
    rec_rsps = []

    duration = 0.0

    for line in data.iter_content(None):

        # Capture Start Time (for duration)
        tstart = time.perf_counter()

        # Process Line Read
        i = 0
        while i < len(line):

            # Parse Record Size
            if(rec_size_index < rec_hdr_size):
                bytes_available = len(line) - i
                bytes_remaining = rec_hdr_size - rec_size_index
                bytes_to_append = min(bytes_available, bytes_remaining)
                rec_size_rsps.append(line[i:i+bytes_to_append])
                rec_size_index += bytes_to_append
                if(rec_size_index >= rec_hdr_size):
                    raw = b''.join(rec_size_rsps)
                    rec_version, rec_type_size, rec_data_size = struct.unpack('>hhi', raw)
                    if rec_version != 2:
                        raise FatalError("Invalid record format: %d" % (rec_version))
                    rec_size = rec_type_size + rec_data_size
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
                    rec     = __decode_native(rectype, rawdata)
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

            # Zero Sized Record
            else:
                rec_size_index = 0
                rec_index = 0

        # Capture Duration
        duration = duration + (time.perf_counter() - tstart)

    # Update Timing Profile
    profiles[__parse_native.__name__] = duration

    return recs

#
#  __build_auth_header
#
def __build_auth_header():
    """
    Build authentication header for use with provisioning system
    """

    global service_url, ps_access_token, ps_refresh_token, ps_token_exp
    headers = None
    if ps_access_token:
        # Check if Refresh Needed
        if time.time() > ps_token_exp:
            host = "https://ps." + service_url + "/api/org_token/refresh/"
            rqst = {"refresh": ps_refresh_token}
            hdrs = {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + ps_access_token}
            rsps = session.post(host, data=json.dumps(rqst), headers=hdrs, timeout=request_timeout).json()
            ps_refresh_token = rsps["refresh"]
            ps_access_token = rsps["access"]
            ps_token_exp =  time.time() + (float(rsps["access_lifetime"]) / 2)
        # Build Authentication Header
        headers = {'Authorization': 'Bearer ' + ps_access_token}
    return headers


###############################################################################
# Default Record Processing
###############################################################################

#
#  __logeventrec
#
def __logeventrec(rec):
    if verbose:
        eventlogger[rec['level']]('%s' % (rec["attr"]))

#
#  __exceptrec
#
def __exceptrec(rec):
    if verbose:
        if rec["code"] >= 0:
            eventlogger[rec["level"]]("Exception <%d>: %s", rec["code"], rec["text"])
        else:
            eventlogger[rec["level"]]("%s", rec["text"])

#
#  _arrowrec
#
def __arrowrec(rec):
    global arrow_file_table
    try :
        filename = rec["filename"]
        if rec["__rectype"] == 'arrowrec.meta':
            if filename in arrow_file_table:
                raise FatalError("file transfer already in progress")
            arrow_file_table[filename] = { "fp": open(filename, "wb"), "size": rec["size"], "progress": 0 }
        else: # rec["__rectype"] == 'arrowrec.data'
            data = rec['data']
            file = arrow_file_table[filename]
            file["fp"].write(bytearray(data))
            file["progress"] += len(data)
            if file["progress"] >= file["size"]:
                file["fp"].close()
                del arrow_file_table[filename]
    except Exception as e:
        raise FatalError("Failed to process arrow file: {}".format(e))

#
#  Globals
#
__callbacks = {'eventrec': __logeventrec, 'exceptrec': __exceptrec, 'arrowrec.meta': __arrowrec, 'arrowrec.data': __arrowrec }

###############################################################################
# APIs
###############################################################################

#
#  source
#
def source (api, parm={}, stream=False, callbacks={}, path="/source"):
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
        path:       str
                    path to api being requested

    Returns
    -------
    dictionary
        response data

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_url("slideruleearth.io")
        >>> rqst = {
        ...     "time": "NOW",
        ...     "input": "NOW",
        ...     "output": "GPS"
        ... }
        >>> rsps = sliderule.source("time", rqst)
        >>> print(rsps)
        {'time': 1300556199523.0, 'format': 'GPS'}
    '''
    global service_url, service_org
    rqst = json.dumps(parm)
    rsps = {}
    headers = None
    # Build Callbacks
    for c in __callbacks:
        if c not in callbacks:
            callbacks[c] = __callbacks[c]
    # Attempt Request
    complete = False
    attempts = 3
    while not complete and attempts > 0:
        attempts -= 1
        try:
            # Construct Request URL and Authorization
            if service_org:
                url = 'https://%s.%s%s/%s' % (service_org, service_url, path, api)
                headers = __build_auth_header()
            else:
                url = 'http://%s%s/%s' % (service_url, path, api)
            # Perform Request
            if not stream:
                data = session.get(url, data=rqst, headers=headers, timeout=request_timeout)
            else:
                data = session.post(url, data=rqst, headers=headers, timeout=request_timeout, stream=True)
            data.raise_for_status()
            # Parse Response
            format = data.headers['Content-Type']
            if format == 'text/plain':
                rsps = __parse_json(data)
            elif format == 'application/json':
                rsps = __parse_json(data)
            elif format == 'application/octet-stream':
                rsps = __parse_native(data, callbacks)
            else:
                raise FatalError('unsupported content type: %s' % (format))
            # Success
            complete = True
        except requests.exceptions.SSLError as e:
            logger.error("Unable to verify SSL certificate: {} ...retrying request".format(e))
        except requests.ConnectionError as e:
            logger.error("Connection error to endpoint {} ...retrying request".format(url))
        except requests.Timeout as e:
            logger.error("Timed-out waiting for response from endpoint {} ...retrying request".format(url))
        except requests.exceptions.ChunkedEncodingError as e:
            logger.error("Unexpected termination of response from endpoint {} ...retrying request".format(url))
        except requests.HTTPError as e:
            if e.response.status_code == 503:
                raise TransientError("Server experiencing heavy load, stalling on request to {}".format(url))
            else:
                raise FatalError("HTTP error {} from endpoint {}".format(e.response.status_code, url))
        except:
            raise
    # Check Success
    if not complete:
        raise FatalError("Unable to complete request due to errors")
    # Return Response
    return rsps

#
#  set_url
#
def set_url (url):
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
    global service_url
    service_url = url

#
#  set_verbose
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
# set_rqst_timeout
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
        raise FatalError('timeout must be a tuple (<connection timeout>, <read timeout>)')

#
# update_available_servers
#
def update_available_servers (desired_nodes=None, time_to_live=None):
    '''
    Manages the number of servers in the cluster.
    If the desired_nodes parameter is set, then a request is made to change the number of servers in the cluster to the number specified.
    In all cases, the number of nodes currently running in the cluster are returned - even if desired_nodes is set;
    subsequent calls to this function is needed to check when the current number of nodes matches the desired_nodes.

    Parameters
    ----------
        desired_nodes:  int
                        the desired number of nodes in the cluster
        time_to_live:   int
                        number of minutes for the desired nodes to run

    Returns
    -------
    int
        number of nodes currently in the cluster
    int
        number of nodes available for work in the cluster

    Examples
    --------
        >>> import sliderule
        >>> num_servers, max_workers = sliderule.update_available_servers(10)
    '''

    global service_url, service_org, request_timeout

    # Update number of nodes
    if type(desired_nodes) == int:
        headers = __build_auth_header()
        if type(time_to_live) == int:
            host = "https://ps." + service_url + "/api/desired_org_num_nodes_ttl/" + service_org + "/" + str(desired_nodes) + "/" + str(time_to_live) + "/"
            rsps = session.post(host, headers=headers, timeout=request_timeout)
        else:
            host = "https://ps." + service_url + "/api/desired_org_num_nodes/" + service_org + "/" + str(desired_nodes) + "/"
            rsps = session.put(host, headers=headers, timeout=request_timeout)
        rsps.raise_for_status()

    # Get number of nodes currently registered
    rsps = source("status", parm={"service":"sliderule"}, path="/discovery")
    available_servers = rsps["nodes"]
    return available_servers, available_servers

#
# authenticate
#
def authenticate (ps_organization, ps_username=None, ps_password=None):
    '''
    Authenticate to SlideRule Provisioning System
    The username and password can be provided the following way in order of priority:
    (1) The passed in arguments `ps_username' and 'ps_password';
    (2) The O.S. environment variables 'PS_USERNAME' and 'PS_PASSWORD';
    (3) The `ps.<url>` entry in the .netrc file in your home directory

    Parameters
    ----------
        ps_organization:    str
                            name of the SlideRule organization the user belongs to

        ps_username:        str
                            SlideRule provisioning system account name

        ps_password:        str
                            SlideRule provisioning system account password
    Returns
    -------
    status
        True of successful, False if unsuccessful

    Examples
    --------
        >>> import sliderule
        >>> sliderule.authenticate("myorg")
        True
    '''
    global service_org, ps_refresh_token, ps_access_token, ps_token_exp
    login_status = False
    ps_url = "ps." + service_url

    # set organization on any authentication request
    service_org = ps_organization

    # check for direct or public access
    if service_org == None:
        return True

    # attempt retrieving from environment
    if not ps_username or not ps_password:
        ps_username = os.environ.get("PS_USERNAME")
        ps_password = os.environ.get("PS_PASSWORD")

    # attempt retrieving from netrc file
    if not ps_username or not ps_password:
        try:
            netrc_file = netrc.netrc()
            login_credentials = netrc_file.hosts[ps_url]
            ps_username = login_credentials[0]
            ps_password = login_credentials[2]
        except Exception as e:
            logger.warning("Failed to retrieve username and password from netrc file: {}".format(e))

    # authenticate to provisioning system
    if ps_username and ps_password:
        rqst = {"username": ps_username, "password": ps_password, "org_name": ps_organization}
        headers = {'Content-Type': 'application/json'}
        try:
            api = "https://" + ps_url + "/api/org_token/"
            rsps = session.post(api, data=json.dumps(rqst), headers=headers, timeout=request_timeout)
            rsps.raise_for_status()
            rsps = rsps.json()
            ps_refresh_token = rsps["refresh"]
            ps_access_token = rsps["access"]
            ps_token_exp =  time.time() + (float(rsps["access_lifetime"]) / 2)
            login_status = True
        except:
            logger.error("Unable to authenticate user %s to %s" % (ps_username, api))

    # return login status
    return login_status

#
# gps2utc
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
# get_definition
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
        >>> sliderule.set_url("slideruleearth.io")
        >>> sliderule.get_definition("atl03rec", "cycle")
        {'fmt': 'H', 'size': 2, 'nptype': <class 'numpy.uint16'>}
    '''
    recdef = __populate(rectype)
    if fieldname in recdef and recdef[fieldname]["type"] in basictypes:
        return basictypes[recdef[fieldname]["type"]]
    else:
        return {}

#
# get_version
#
def get_version ():
    '''
    Get the version information for the running servers and Python client

    Returns
    -------
    dict
        dictionary of version information
    '''
    rsps = source("version", {})
    rsps["client"] = {"version": version.full_version}
    return rsps

#
# check_version
#
def check_version (plugins=[]):
    '''
    Check that the version of the client matches the version of the server and any additionally requested plugins

    Parameters
    ----------
    plugins:    list
                list of package names (as strings) to check the version on

    Returns
    -------
    bool
        True if at least minor version matches; False if major or minor version doesn't match
    '''
    status = True
    info = get_version()
    # populate version info
    versions = {}
    for entity in ['server', 'client'] + plugins:
        s = info[entity]['version'][1:].split('.')
        versions[entity] = (int(s[0]), int(s[1]), int(s[2]))
    # check major version mismatches
    if versions['server'][0] != versions['client'][0]:
        raise RuntimeError("Client (version {}) is incompatible with the server (version {})".format(versions['server'], versions['client']))
    else:
        for pkg in plugins:
            if versions[pkg][0] != versions['client'][0]:
                raise RuntimeError("Client (version {}) is incompatible with the {} plugin (version {})".format(versions['server'], pkg, versions['icesat2']))
    # check minor version mismatches
    if versions['server'][1] > versions['client'][1]:
        logger.warning("Client (version {}) is out of date with the server (version {})".format(versions['server'], versions['client']))
        status = False
    else:
        for pkg in plugins:
            if versions[pkg][1] > versions['client'][1]:
                logger.warning("Client (version {}) is out of date with the {} plugin (version {})".format(versions['server'], pkg, versions['client']))
                status = False
    # return if version check is successful
    return status
