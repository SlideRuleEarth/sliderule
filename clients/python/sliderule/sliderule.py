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
import socket
import json
import struct
import ctypes
import time
import logging
import warnings
import numpy
import geopandas
from shapely.geometry import Polygon
from shapely.geometry.multipolygon import MultiPolygon
from datetime import datetime
from sliderule import version

###############################################################################
# GLOBALS
###############################################################################

# Coordinate Reference system definition for WGS84 Ellipsoid ensemble
# Coordinate system on the surface of a sphere or ellipsoid of reference.
EPSG_WGS84 = "EPSG:4326"

# This is 3D CRS for ITRF2014 realization with 2010.0 epoch: https://epsg.io/7912
# Note: using GRS80 ellipsoid, negligible difference in inverse flattening from WGS84
SLIDERULE_EPSG = "EPSG:7912"

PUBLIC_URL = "slideruleearth.io"
PUBLIC_ORG = "sliderule"

DEFAULT_TRUST_ENV = False

service_url = PUBLIC_URL
service_org = PUBLIC_ORG

session = requests.Session()
session.trust_env = DEFAULT_TRUST_ENV

ps_refresh_token = None
ps_access_token = None
ps_token_exp = None

MAX_PS_CLUSTER_WAIT_SECS = 600

request_timeout = (10, 120) # (connection, read) in seconds
decode_aux = True

logger = logging.getLogger(__name__)
console = None

rethrow_exceptions = False

clustering_enabled = False
try:
    from sklearn.cluster import KMeans
    clustering_enabled = True
except:
    logger.info("Unable to import sklearn... clustering support disabled")

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

exceptioncodes = {
    "INFO": 0,
    "ERROR": -1,
    "TIMEOUT": -2,
    "RESOURCE_DOES_NOT_EXIST": -3,
    "EMPTY_SUBSET": -4,
    "SIMPLIFY": -5
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
    "TIME8":    { "fmt": 'q', "size": 8, "nptype": numpy.int64  }, # numpy.datetime64
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

class RetryRequest(RuntimeError):
    pass

###############################################################################
# UTILITIES
###############################################################################

#
# __StreamSource
#
class __StreamSource:
    def __init__(self, data):
        self.source = data
    def __iter__(self):
        for line in self.source.iter_content(None):
            yield line

#
# __BufferSource
#
class __BufferSource:
    def __init__(self, data):
        self.source = data
    def __iter__(self):
        yield self.source

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
    for line in data:
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

        # check for mvp flag
        if not decode_aux and "AUX" in flags:
            continue

        # get endianness
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

    for line in data:

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
                    if rectype == "conrec":
                        # parse records contained in record
                        buffer = __BufferSource(rawdata[rec["start"]:])
                        contained_recs = __parse_native(buffer, callbacks)
                        recs += contained_recs
                    else:
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


###############################################################################
# Overriding DNS
###############################################################################

#
# local_dns - global dictionary of DNS entries to override
#
local_dns = {}

#
# __initdns - resets the global dictionary
#
def __initdns():
    local_dns.clear()

#
# __dnsoverridden - checks if url is already overridden
#
def __dnsoverridden():
    global service_org, service_url
    url = service_org + "." + service_url
    return url.lower() in local_dns

#
# __jamdns - override the dns entry
#
def __jamdns():
    global service_url, service_org, request_timeout
    url = service_org + "." + service_url
    headers = buildauthheader()
    host = "https://ps." + service_url + "/api/org_ip_adr/" + service_org + "/"
    rsps = session.get(host, headers=headers, timeout=request_timeout).json()
    if rsps["status"] == "SUCCESS":
        ipaddr = rsps["ip_address"]
        local_dns[url.lower()] = ipaddr
        logger.info("Overriding DNS for {} with {}".format(url, ipaddr))

#
# __override_getaddrinfo - replaces the socket library callback
#
socket_getaddrinfo = socket.getaddrinfo
def __override_getaddrinfo(*args):
    url = args[0].lower()
    if url in local_dns:
        logger.info("getaddrinfo returned {} for {}".format(local_dns[url], url))
        return socket_getaddrinfo(local_dns[url], *args[1:])
    else:
        return socket_getaddrinfo(*args)
socket.getaddrinfo = __override_getaddrinfo


###############################################################################
# Default Record Processing
###############################################################################

#
#  __logeventrec
#
def __logeventrec(rec):
    eventlogger[rec['level']]('Log <%s, %d>: %s' % (rec["name"], rec["time"], rec["attr"]))

#
#  __exceptrec
#
def __exceptrec(rec):
    if rec["code"] == exceptioncodes["SIMPLIFY"]:
        raise RetryRequest("cmr simplification requested")
    elif rec["code"] < 0:
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
                raise FatalError(f'File transfer already in progress for {filename}')
            logger.info(f'Writing arrow file: {filename}')
            arrow_file_table[filename] = { "fp": open(filename, "wb"), "size": rec["size"], "progress": 0 }
        elif rec["__rectype"] == 'arrowrec.eof':
            logger.info(f'Checksum of arrow file: {rec["checksum"]}')
        else: # rec["__rectype"] == 'arrowrec.data'
            data = rec['data']
            file = arrow_file_table[filename]
            file["fp"].write(bytearray(data))
            file["progress"] += len(data)
            if file["progress"] >= file["size"]:
                file["fp"].close()
                logger.info(f'Closing arrow file: {filename}')
                del arrow_file_table[filename]
    except Exception as e:
        raise FatalError(f'Failed to process arrow file: {e}')

#
#  Globals
#
__callbacks = {'eventrec': __logeventrec, 'exceptrec': __exceptrec, 'arrowrec.meta': __arrowrec, 'arrowrec.data': __arrowrec, 'arrowrec.eof': __arrowrec}


###############################################################################
# INTERNAL APIs
###############################################################################

#
#  buildauthheader
#
def buildauthheader(force_refresh=False):
    """
    Build authentication header for use with provisioning system
    """
    global service_url, ps_access_token, ps_refresh_token, ps_token_exp
    headers = None
    if ps_access_token:
        # Check if Refresh Needed
        if time.time() > ps_token_exp or force_refresh:
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

#
# GeoDataFrame to Polygon
#
def gdf2poly(gdf):

    # latch start time
    tstart = time.perf_counter()

    # pull out coordinates
    hull = gdf.unary_union.convex_hull
    polygon = [{"lon": coord[0], "lat": coord[1]} for coord in list(hull.exterior.coords)]

    # determine winding of polygon #
    #              (x2               -    x1)             *    (y2               +    y1)
    wind = sum([(polygon[i+1]["lon"] - polygon[i]["lon"]) * (polygon[i+1]["lat"] + polygon[i]["lat"]) for i in range(len(polygon) - 1)])
    if wind > 0:
        # reverse direction (make counter-clockwise) #
        ccw_poly = []
        for i in range(len(polygon), 0, -1):
            ccw_poly.append(polygon[i - 1])
        # replace region with counter-clockwise version #
        polygon = ccw_poly

    # Update Profile
    profiles[gdf2poly.__name__] = time.perf_counter() - tstart

    # return polygon
    return polygon

#
#  Create Empty GeoDataFrame
#
def emptyframe(**kwargs):
    # set default keyword arguments
    kwargs['crs'] = SLIDERULE_EPSG
    return geopandas.GeoDataFrame(geometry=geopandas.points_from_xy([], [], []), crs=kwargs['crs'])

#
# Process Output File
#
def procoutputfile(parm, rsps):
    output = parm["output"]
    if "path" in output:
        path = output["path"]
    else:
        path = None
    # Check If Remote Record Is In Responses
    for rsp in rsps:
        if 'arrowrec.remote' == rsp['__rectype']:
            path = rsp['url']
            break
    # Handle Local Files
    if "open_on_complete" in output and output["open_on_complete"]:
        if output["format"] == "parquet":
            if "as_geo" in output and output["as_geo"]:
                # Return GeoParquet File as GeoDataFrame
                return geopandas.read_parquet(path)
            else:
                # Return Parquet File as DataFrame
                return geopandas.pd.read_parquet(path)
        elif output["format"] == "geoparquet":
            # Return Parquet File as DataFrame
            return geopandas.pd.read_parquet(path)
        elif output["format"] == "feather":
            # Return Feather File as DataFrame
            return geopandas.pd.read_feather(path)
        elif output["format"] == "csv":
            # Return CSV File as DataFrame
            return geopandas.pd.read_csv(path)
        else:
            raise FatalError('unsupported output format: %s' % (output["format"]))
    else:
        # Return Parquet Filename
        return path

#
#  Get Values from Raw Buffer
#
def getvalues(data, dtype, size, num_elements=0):
    """
    data:   tuple of bytes
    dtype:  element of codedtype
    size:   bytes in data
    """

    raw = bytes(data)
    datatype = basictypes[codedtype2str[dtype]]["nptype"]
    if num_elements == 0: # dynamically determine number of elements
        num_elements = int(size / numpy.dtype(datatype).itemsize)
    slicesize = num_elements * numpy.dtype(datatype).itemsize # truncates partial bytes
    values = numpy.frombuffer(raw[:slicesize], dtype=datatype, count=num_elements)

    return values

#
#  Dictionary to GeoDataFrame
#
def todataframe(columns, time_key="time", lon_key="longitude", lat_key="latitude", height_key=None, **kwargs):

    # Latch Start Time
    tstart = time.perf_counter()

    # Set Default Keyword Arguments
    kwargs['index_key'] = "time"
    kwargs['crs'] = SLIDERULE_EPSG

    # Check Empty Columns
    if len(columns) <= 0:
        return emptyframe(**kwargs)

    # Generate Time Column
    columns['time'] = columns[time_key].astype('datetime64[ns]')

    # Generate Geometry Column
    # 3D point geometry
    # This enables 3D CRS transformations using the to_crs() method
    if height_key == None:
        geometry = geopandas.points_from_xy(columns[lon_key], columns[lat_key])
    else:
        geometry = geopandas.points_from_xy(columns[lon_key], columns[lat_key], columns[height_key])
    del columns[lon_key]
    del columns[lat_key]

    # Create Pandas DataFrame object
    if type(columns) == dict:
        df = geopandas.pd.DataFrame(columns)
    else:
        df = columns

    # Build GeoDataFrame (default geometry is crs=SLIDERULE_EPSG)
    gdf = geopandas.GeoDataFrame(df, geometry=geometry, crs=kwargs['crs'])

    # Set index (default is Timestamp), can add `verify_integrity=True` to check for duplicates
    # Can do this during DataFrame creation, but this allows input argument for desired column
    gdf.set_index(kwargs['index_key'], inplace=True)

    # Sort values for reproducible output despite async processing
    gdf.sort_index(inplace=True)

    # Update Profile
    profiles[todataframe.__name__] = time.perf_counter() - tstart

    # Return GeoDataFrame
    return gdf

#
# Simplify Polygon
#
def simplifypolygon(parm):
    if "parms" not in parm:
        return

    if "cmr" in parm["parms"]:
        polygon = parm["parms"]["cmr"]["polygon"]
    elif "poly" in parm["parms"]:
        polygon = parm["parms"]["poly"]
    else:
        return

    tolerance = 0.01
    raw_multi_polygon = [[(tuple([(c['lon'], c['lat']) for c in polygon]), [])]]
    shape = MultiPolygon(*raw_multi_polygon)
    buffered_shape = shape.buffer(tolerance)
    simplified_shape = buffered_shape.simplify(tolerance)
    simplified_coords = list(simplified_shape.exterior.coords)

    simplified_polygon = []
    for coord in simplified_coords:
        point = {"lon": coord[0], "lat": coord[1]}
        simplified_polygon.insert(0, point)

    if "cmr" not in parm["parms"]:
        parm["parms"]["cmr"] = {}
    parm["parms"]["cmr"]["polygon"] = simplified_polygon

    logger.warning('Using simplified polygon (for CMR request only!), {} points using tolerance of {}'.format(len(simplified_coords), tolerance))

#
# rethrow
#
def rethrow():
    global rethrow_exceptions
    return rethrow_exceptions

###############################################################################
# APIs
###############################################################################

#
#  Initialize
#
def init (
    url=PUBLIC_URL,
    verbose=False,
    loglevel=logging.INFO,
    organization=0,
    desired_nodes=None,
    time_to_live=60,
    bypass_dns=False,
    plugins=[],
    trust_env=DEFAULT_TRUST_ENV,
    log_handler=None,
    rethrow=None
):
    '''
    Initializes the Python client for use with SlideRule, and should be called before other ICESat-2 API calls.
    This function is a wrapper for a handful of sliderule functions that would otherwise all have to be called in order to initialize the client.

    Parameters
    ----------
        url:            str
                        the IP address or hostname of the SlideRule service (slidereearth.io by default)
        verbose:        bool
                        sets up console logger as a convenience to user so all logs are printed to screen
        loglevel:       int
                        minimum severity of log message to output
        organization:   str
                        SlideRule provisioning system organization the user belongs to (see sliderule.authenticate for details)
        desired_nodes:  int
                        requested number of processing nodes in the cluster
        time_to_live:   int
                        minimum number of minutes the desired number of nodes should be present in the cluster
        bypass_dns:     bool
                        if true then the ip address for the cluster is retrieved from the provisioning system and used directly
        plugins:        list
                        names of the plugins that need to be available on the server
        log_handler:    logger
                        user provided logging handler
        rethrow:        bool
                        configure client to throw any exceptions it encounters instead of automatic retries

    Returns
    -------
    bool
        Status of version check

    Examples
    --------
        >>> import sliderule
        >>> sliderule.init()
    '''
    global session, logger, rethrow_exceptions
    # massage function parameters
    if organization == 0:
        organization = PUBLIC_ORG
    # reconfigure session (if necessary)
    if session.trust_env != trust_env:
        session.trust_env = trust_env
    # configure logging
    if log_handler != None:
        logger.addHandler(log_handler)
    set_verbose(verbose, loglevel)
    # configure client
    if rethrow != None:
        rethrow_exceptions = rethrow
    set_url(url) # configure domain
    authenticate(organization) # configure credentials (if any) for organization
    scaleout(desired_nodes, time_to_live, bypass_dns) # set cluster to desired number of nodes (if permitted based on credentials)
    return check_version(plugins=plugins) # verify compatibility between client and server versions

#
#  source
#
def source (api, parm={}, stream=False, callbacks={}, path="/source", silence=False):
    '''
    Perform API call to SlideRule service

    Parameters
    ----------
        api:        str
                    name of the SlideRule endpoint
        parm:       dict
                    dictionary of request parameters
        stream:     bool
                    whether the request is a **normal** service or a **stream** service (see `De-serialization </web/rtd/user_guide/SlideRule.html#de-serialization>`_ for more details)
        callbacks:  dict
                    record type callbacks (advanced use)
        path:       str
                    path to api being requested
        silence:    bool
                    whether error log messages should be generated

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
    global service_url, service_org, rethrow_exceptions
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
            # Dump Parameters to Request JSON String
            rqst = json.dumps(parm)
            # Construct Request URL and Authorization
            if service_org:
                url = 'https://%s.%s%s/%s' % (service_org, service_url, path, api)
                headers = buildauthheader()
            else:
                url = 'http://%s%s/%s' % (service_url, path, api)
            # Perform Request
            if not stream:
                data = session.get(url, data=rqst, headers=headers, timeout=request_timeout)
            else:
                data = session.post(url, data=rqst, headers=headers, timeout=request_timeout, stream=True)
            data.raise_for_status()
            # Parse Response
            stream = __StreamSource(data)
            format = data.headers['Content-Type']
            if format == 'text/plain':
                rsps = __parse_json(stream)
            elif format == 'application/json':
                rsps = __parse_json(stream)
            elif format == 'application/octet-stream':
                rsps = __parse_native(stream, callbacks)
            else:
                raise FatalError('unsupported content type: %s' % (format))
            # Success
            complete = True
        except RetryRequest as e:
            logger.info("Retry requested by {}: {}".format(url, e))
            simplifypolygon(parm)
        except requests.exceptions.SSLError as e:
            logger.debug("Exception in request to {}: {}".format(url, e))
            if rethrow_exceptions:
                raise
            if not silence:
                logger.error("Unable to verify SSL certificate for {} ...retrying request".format(url))
        except requests.ConnectionError as e:
            logger.debug("Exception in request to {}: {}".format(url, e))
            if rethrow_exceptions:
                raise
            if not silence:
                logger.error("Connection error to endpoint {} ...retrying request".format(url))
        except requests.Timeout as e:
            logger.debug("Exception in request to {}: {}".format(url, e))
            if rethrow_exceptions:
                raise
            if not silence:
                logger.error("Timed-out waiting for response from endpoint {} ...retrying request".format(url))
        except requests.exceptions.ChunkedEncodingError as e:
            logger.debug("Exception in request to {}: {}".format(url, e))
            if rethrow_exceptions:
                raise
            if not silence:
                logger.error("Unexpected termination of response from endpoint {} ...retrying request".format(url))
        except requests.HTTPError as e:
            logger.debug("Exception in request to {}: {}".format(url, e))
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
    logger.info(f'Setting URL to {service_url}')

#
#  set_verbose
#
def set_verbose (enable, loglevel=logging.INFO):
    '''
    Sets up a console logger to print log messages to screen

    If you want more control over the behavior of the log messages being captured, do not call this function but instead
    create and configure a Python log handler of your own and attach it to `sliderule.logger`.

    Parameters
    ----------
        enable:     bool
                    True: creates console logger if it doesn't exist, False: destroys console logger if it does exist

        loglevel:       int
                        minimum severity of log message to output

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_verbose(True, loglevel=logging.INFO)
    '''
    global console, logger
    # massage loglevel parameter if passed in as a string
    if loglevel == "DEBUG":
        loglevel = logging.DEBUG
    elif loglevel == "INFO":
        loglevel = logging.INFO
    elif loglevel == "WARNING":
        loglevel = logging.WARNING
    elif loglevel == "WARN":
        loglevel = logging.WARN
    elif loglevel == "ERROR":
        loglevel = logging.ERROR
    elif loglevel == "FATAL":
        loglevel = logging.FATAL
    elif loglevel == "CRITICAL":
        loglevel = logging.CRITICAL
    # enable/disable logging to console
    if (enable == True) and (console == None):
        console = logging.StreamHandler()
        logger.addHandler(console)
    elif (enable == False) and (console != None):
        logger.removeHandler(console)
        console = None
    # always set level to requested
    logger.setLevel(loglevel)
    if console != None:
        console.setLevel(loglevel)

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
# set_processing_flags
#
def set_processing_flags (aux=True):
    '''
    Sets flags used when processing the record definitions

    Parameters
    ----------
        aux:    bool
                decode auxiliary fields

    Examples
    --------
        >>> import sliderule
        >>> sliderule.set_processing_flags(aux=False)
    '''
    global decode_aux
    if type(aux) == bool:
        decode_aux = aux
    else:
        raise FatalError('aux must be a boolean')


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
    global service_url, service_org, request_timeout, rethrow_exceptions
    requested_nodes = 0

    # Update number of nodes
    if type(desired_nodes) == int:
        rsps_body = {}
        requested_nodes = desired_nodes
        headers = buildauthheader()

        # Get boundaries of cluster and calculate nodes to request
        try:
            host = "https://ps." + service_url + "/api/org_num_nodes/" + service_org + "/"
            rsps = session.get(host, headers=headers, timeout=request_timeout)
            rsps_body = rsps.json()
            rsps.raise_for_status()
            requested_nodes = max(min(desired_nodes, rsps_body["max_nodes"]), rsps_body["min_nodes"])
            if requested_nodes != desired_nodes:
                logger.info("Provisioning system desired nodes overridden to {}".format(requested_nodes))
        except requests.exceptions.HTTPError as e:
            logger.info('{}'.format(e))
            logger.info('Provisioning system status request returned error => {}'.format(rsps_body["error_msg"]))
            if rethrow_exceptions:
                raise

        # Request number of nodes in cluster
        try:
            if type(time_to_live) == int:
                host = "https://ps." + service_url + "/api/desired_org_num_nodes_ttl/" + service_org + "/" + str(requested_nodes) + "/" + str(time_to_live) + "/"
                rsps = session.post(host, headers=headers, timeout=request_timeout)
            else:
                host = "https://ps." + service_url + "/api/desired_org_num_nodes/" + service_org + "/" + str(requested_nodes) + "/"
                rsps = session.put(host, headers=headers, timeout=request_timeout)
            rsps_body = rsps.json()
            rsps.raise_for_status()
        except requests.exceptions.HTTPError as e:
            logger.info('{}'.format(e))
            logger.error('Provisioning system update request error => {}'.format(rsps_body["error_msg"]))
            if rethrow_exceptions:
                raise

    # Get number of nodes currently registered
    try:
        rsps = source("status", parm={"service":"sliderule"}, path="/discovery", silence=True)
        available_servers = rsps["nodes"]
    except FatalError as e:
        logger.debug("Failed to retrieve number of nodes registered: {}".format(e))
        available_servers = 0

    return available_servers, requested_nodes

#
# scaleout
#
def scaleout(desired_nodes, time_to_live, bypass_dns):
    '''
    Scale the cluster and wait for cluster to reach desired state

    Parameters
    ----------
        desired_nodes:  int
                        the desired number of nodes in the cluster
        time_to_live:   int
                        number of minutes for the desired nodes to run
        bypass_dns:     bool
                        the cluster ip address is retrieved from the provisioning system and used directly

    Examples
    --------
        >>> import sliderule
        >>> sliderule.scaleout(4, 300, False)
    '''
    if desired_nodes is None:
        return # nothing needs to be done
    if desired_nodes < 0:
        raise FatalError("Number of desired nodes must be greater than zero ({})".format(desired_nodes))
    # Initialize DNS
    __initdns() # clear cache of DNS lookups for clusters
    if bypass_dns:
        __jamdns() # use ip address for cluster
    # Send Initial Request for Desired Cluster State
    start = time.time()
    available_nodes,requested_nodes = update_available_servers(desired_nodes=desired_nodes, time_to_live=time_to_live)
    scale_up_needed = False
    # Wait for Cluster to Reach Desired State
    while available_nodes < requested_nodes:
        scale_up_needed = True
        logger.info("Waiting while cluster scales to desired capacity (currently at {} nodes, desired is {} nodes)... {} seconds".format(available_nodes, desired_nodes, int(time.time() - start)))
        time.sleep(10.0)
        available_nodes,_ = update_available_servers()
        # Override DNS if Cluster is Starting
        if available_nodes == 0 and not __dnsoverridden():
            __jamdns()
        # Timeout Occurred
        if int(time.time() - start) > MAX_PS_CLUSTER_WAIT_SECS:
            logger.error("Maximum time allowed waiting for cluster has been exceeded")
            break
    # Log Final Message if Cluster Needed State Change
    if scale_up_needed:
        logger.info("Cluster has reached capacity of {} nodes... {} seconds".format(available_nodes, int(time.time() - start)))

#
# authenticate
#
def authenticate (ps_organization, ps_username=None, ps_password=None, github_token=None):
    '''
    Authenticate to SlideRule Provisioning System
    The username and password can be provided the following way in order of priority:
    (1) The passed in arguments `github_token` or `ps_username` and `ps_password`;
    (2) The O.S. environment variables `PS_GITHUB_TOKEN` or `PS_USERNAME` and `PS_PASSWORD`;
    (3) The `ps.<url>` entry in the .netrc file in your home directory

    Parameters
    ----------
        ps_organization:    str
                            name of the SlideRule organization the user belongs to

        ps_username:        str
                            SlideRule provisioning system account name

        ps_password:        str
                            SlideRule provisioning system account password

        github_token:       str
                            GitHub access token (minimum scope/permissions require)

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
    if not github_token and not ps_username and not ps_password:
        github_token = os.environ.get("PS_GITHUB_TOKEN")
        ps_username = os.environ.get("PS_USERNAME")
        ps_password = os.environ.get("PS_PASSWORD")

    # attempt retrieving from netrc file
    if not github_token and not ps_username and not ps_password:
        try:
            netrc_file = netrc.netrc()
            login_credentials = netrc_file.hosts[ps_url]
            ps_username = login_credentials[0]
            ps_password = login_credentials[2]
        except Exception as e:
            if ps_organization != PUBLIC_ORG:
                logger.warning("Unable to retrieve username and password from netrc file for machine: {}".format(e))

    # build authentication request
    user = None
    if github_token:
        user = "github"
        rqst = {"org_name": ps_organization, "access_token": github_token}
        headers = {'Content-Type': 'application/json'}
        api = "https://" + ps_url + "/api/org_token_github/"
    elif ps_username or ps_password:
        user = "local"
        rqst = {"username": ps_username, "password": ps_password, "org_name": ps_organization}
        headers = {'Content-Type': 'application/json'}
        api = "https://" + ps_url + "/api/org_token/"

    # authenticate to provisioning system
    if user:
        try:
            rsps = session.post(api, data=json.dumps(rqst), headers=headers, timeout=request_timeout)
            rsps.raise_for_status()
            rsps = rsps.json()
            ps_refresh_token = rsps["refresh"]
            ps_access_token = rsps["access"]
            ps_token_exp =  time.time() + (float(rsps["access_lifetime"]) / 2)
            login_status = True
        except:
            if ps_organization != PUBLIC_ORG:
                logger.error("Unable to authenticate %s user to %s" % (user, api))

    # return login status
    logger.info(f'Login status to {service_url}/{service_org}: {login_status and "success" or "failure"}')
    return login_status

#
# gps2utc
#
def gps2utc (gps_time, as_str=True):
    '''
    Convert a GPS based time returned from SlideRule into a UTC time.

    Parameters
    ----------
        gps_time:   float
                    number of seconds since GPS epoch (January 6, 1980)
        as_str:     bool
                    if True, returns the time as a string; if False, returns the time as datatime object

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
    rsps = source("time", {"time": int(gps_time * 1000), "input": "GPS", "output": "DATE"})
    if as_str:
        return rsps["time"]
    else:
        return datetime.strptime(rsps["time"], '%Y-%m-%dT%H:%M:%SZ')
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
    global service_org
    rsps = source("version", {})
    rsps["client"] = {"version": version.full_version}
    rsps["organization"] = service_org
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
        raise RuntimeError("Client (version {}) is incompatible with the server (version {})".format(versions['client'], versions['server']))
    else:
        for pkg in plugins:
            if versions[pkg][0] != versions['client'][0]:
                raise RuntimeError("Client (version {}) is incompatible with the {} plugin (version {})".format(versions['client'], pkg, versions[pkg]))
    # check minor version mismatches
    if versions['server'][1] > versions['client'][1]:
        logger.warning("Client (version {}) is out of date with the server (version {})".format(versions['client'], versions['server']))
        status = False
    else:
        for pkg in plugins:
            if versions[pkg][1] > versions['client'][1]:
                logger.warning("Client (version {}) is out of date with the {} plugin (version {})".format(versions['client'], pkg, versions['server']))
                status = False
    # return if version check is successful
    return status

#
# Format Region Specification
#
def toregion(source, tolerance=0.0, cellsize=0.01, n_clusters=1):
    '''
    Convert a GeoJSON/Shapefile/GeoDataFrame/list representation of a set of geospatial regions into a list of lat,lon coordinates and raster image recognized by SlideRule

    Parameters
    ----------
        source:     str
                    file name of GeoJSON formatted regions of interest, file **must** have name with the .geojson suffix
                    file name of ESRI Shapefile formatted regions of interest, file **must** have name with the .shp suffix
                    GeoDataFrame of region of interest
                    list of longitude,latitude pairs forming a polygon (e.g. [lat1, lon1, lat2, lon2, lat3, lon3, lat1, lon1])
                    list of longitude,latitude pairs forming a bounding box (e.g. [lat1, lon1, lat2, lon2])
        tolerance:  float
                    tolerance used to simplify complex shapes so that the number of points is less than the limit (a tolerance of 0.001 typically works for most complex shapes)
        cellsize:   float
                    size of pixel in degrees used to create the raster image of the polygon
        n_clusters: int
                    number of clusters of polygons to create when breaking up the request to CMR

    Returns
    -------
    dict
        a list of longitudes and latitudes containing the region of interest that can be used for the **poly** and **raster** parameters in a processing request to SlideRule.

        region = {

            "gdf": <GeoDataFrame of region>

            "poly": [{"lat": <lat1>, "lon": <lon1> }, ...],

            "raster": {"data": <geojson file as string>,

            "clusters": [[{"lat": <lat1>, "lon": <lon1>}, ...], [{"lat": <lat1>, "lon": <lon1>}, ...]] }

    Examples
    --------
    >>> import sliderule, json
    >>> region = sliderule.toregion("tests/data/grandmesa.geojson")
    >>> print(json.dumps(region["poly"], indent=4))
    [
        {
            "lon": -108.20772968780051,
            "lat": 38.8232055291981
        },
        {
            "lon": -108.07460164311031,
            "lat": 38.8475137825863
        },
        {
            "lon": -107.72839858755752,
            "lat": 39.01510930230633
        },
        {
            "lon": -107.78724142490994,
            "lat": 39.195630349659986
        },
        {
            "lon": -108.17287000970857,
            "lat": 39.15920066396116
        },
        {
            "lon": -108.31168256553767,
            "lat": 39.13757646212944
        },
        {
            "lon": -108.34115668325224,
            "lat": 39.03758987613325
        },
        {
            "lon": -108.2878686387796,
            "lat": 38.89051431295789
        },
        {
            "lon": -108.20772968780051,
            "lat": 38.8232055291981
        }
    ]
    '''

    tstart = time.perf_counter()
    tempfile = "temp.geojson"

    if isinstance(source, geopandas.GeoDataFrame):
        # user provided GeoDataFrame instead of a file
        gdf = source
        # Convert to geojson file
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, Polygon):
        gdf = geopandas.GeoDataFrame(geometry=[source], crs=EPSG_WGS84)
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, list) and (len(source) >= 4) and (len(source) % 2 == 0):
        # create lat/lon lists
        if len(source) == 4: # bounding box
            lons = [source[0], source[2], source[2], source[0], source[0]]
            lats = [source[1], source[1], source[3], source[3], source[1]]
        elif len(source) > 4: # polygon list
            lons = [source[i] for i in range(1,len(source),2)]
            lats = [source[i] for i in range(0,len(source),2)]

        # create geodataframe
        p = Polygon([point for point in zip(lons, lats)])
        gdf = geopandas.GeoDataFrame(geometry=[p], crs=EPSG_WGS84)

        # Convert to geojson file
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, str) and (source.find(".shp") > 1):
        # create geodataframe
        gdf = geopandas.read_file(source)
        # Convert to geojson file
        gdf.to_file(tempfile, driver="GeoJSON")
        with open(tempfile, mode='rt') as file:
            datafile = file.read()
        os.remove(tempfile)

    elif isinstance(source, str) and (source.find(".geojson") > 1):
        # create geodataframe
        gdf = geopandas.read_file(source)
        with open(source, mode='rt') as file:
            datafile = file.read()

    else:
        raise FatalError("incorrect filetype: please use a .geojson, .shp, or a geodataframe")


    # If user provided raster we don't have gdf, geopandas cannot easily convert it
    polygon = clusters = None
    if gdf is not None:
        # simplify polygon
        if(tolerance > 0.0):
            with warnings.catch_warnings():
                warnings.simplefilter("ignore")
                gdf = gdf.buffer(tolerance)
                gdf = gdf.simplify(tolerance)

        # generate polygon
        polygon = gdf2poly(gdf)

        # generate clusters
        clusters = []
        if n_clusters > 1:
            if clustering_enabled:
                # pull out centroids of each geometry object
                if "CenLon" in gdf and "CenLat" in gdf:
                    X = numpy.column_stack((gdf["CenLon"], gdf["CenLat"]))
                else:
                    s = gdf.centroid
                    X = numpy.column_stack((s.x, s.y))
                # run k means clustering algorithm against polygons in gdf
                kmeans = KMeans(n_clusters=n_clusters, init='k-means++', random_state=5, max_iter=400)
                y_kmeans = kmeans.fit_predict(X)
                k = geopandas.pd.DataFrame(y_kmeans, columns=['cluster'])
                gdf = gdf.join(k)
                # build polygon for each cluster
                for n in range(n_clusters):
                    c_gdf = gdf[gdf["cluster"] == n]
                    c_poly = gdf2poly(c_gdf)
                    clusters.append(c_poly)
            else:
                raise FatalError("Clustering support not enabled; unable to import sklearn package")

    # update timing profiles
    profiles[toregion.__name__] = time.perf_counter() - tstart

    # return region
    return {
        "gdf": gdf,
        "poly": polygon, # convex hull of polygons
        "clusters": clusters, # list of polygon clusters for cmr request
        "raster": {
            "geojson": datafile, # geojson file
            "length": len(datafile), # geojson file length
            "cellsize": cellsize  # units are in crs/projection
        }
    }
