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
import numpy
from sliderule import version

###############################################################################
# LOGGING
###############################################################################

logger = logging.getLogger(__name__)
eventlogger = {
    0: logger.debug,
    1: logger.info,
    2: logger.warning,
    3: logger.error,
    4: logger.critical
}

###############################################################################
# DNS OVERRIDE
###############################################################################

sliderule_getaddrinfo = socket.getaddrinfo
sliderule_dns = {}

def __override_getaddrinfo(*args, **kwargs):
    global sliderule_getaddrinfo, sliderule_dns
    url = args[0].lower()
    if url in sliderule_dns:
        logger.debug("getaddrinfo returned {} for {}".format(sliderule_dns[url], url))
        return sliderule_getaddrinfo(sliderule_dns[url], *args[1:], **kwargs)
    else:
        return sliderule_getaddrinfo(*args, **kwargs)

socket.getaddrinfo = __override_getaddrinfo

###############################################################################
# GLOBALS
###############################################################################

EXCEPTION_CODES = {
    "INFO": 0,
    "ERROR": -1,
    "TIMEOUT": -2,
    "RESOURCE_DOES_NOT_EXIST": -3,
    "EMPTY_SUBSET": -4,
    "SIMPLIFY": -5
}

BASIC_TYPES = {
    "INT8":     { "fmt": 'b', "size": 1, "nptype": numpy.int8   },
    "INT16":    { "fmt": 'h', "size": 2, "nptype": numpy.int16  },
    "INT32":    { "fmt": 'i', "size": 4, "nptype": numpy.int32  },
    "INT64":    { "fmt": 'q', "size": 8, "nptype": numpy.int64  },
    "UINT8":    { "fmt": 'B', "size": 1, "nptype": numpy.uint8  },
    "UINT16":   { "fmt": 'H', "size": 2, "nptype": numpy.uint16 },
    "UINT32":   { "fmt": 'I', "size": 4, "nptype": numpy.uint32 },
    "UINT64":   { "fmt": 'Q', "size": 8, "nptype": numpy.uint64 },
    "BITFIELD": { "fmt": 'x', "size": 0, "nptype": numpy.byte   }, # unsupported
    "FLOAT":    { "fmt": 'f', "size": 4, "nptype": numpy.single },
    "DOUBLE":   { "fmt": 'd', "size": 8, "nptype": numpy.double },
    "TIME8":    { "fmt": 'q', "size": 8, "nptype": numpy.int64  }, # numpy.datetime64
    "STRING":   { "fmt": 's', "size": 1, "nptype": numpy.byte   }
}

CODED_TYPE = {
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

#
# FatalError
#
class FatalError(RuntimeError):
    pass

#
# RetryRequest
#
class RetryRequest(RuntimeError):
    pass

#
# Session
#
class Session:

    PUBLIC_URL = "slideruleearth.io"
    PUBLIC_ORG = "sliderule"
    MAX_PS_CLUSTER_WAIT_SECS = 600

    #
    # constructor
    #
    def __init__ (self,
        domain          = PUBLIC_URL,
        verbose         = False,
        loglevel        = logging.INFO,
        organization    = 0,
        desired_nodes   = None,
        time_to_live    = 60, # minutes
        bypass_dns      = False,
        trust_env       = False,
        ssl_verify      = True,
        rqst_timeout    = (10, 120), # (connection, read) in seconds
        log_handler     = None,
        decode_aux      = True,
        rethrow         = False ):
        '''
        creates and configures a sliderule session
        '''
        # configure parameter arguments
        self.session = requests.Session()
        self.session.trust_env = trust_env
        self.ssl_verify = ssl_verify
        self.rqst_timeout = rqst_timeout
        self.decode_aux = decode_aux
        self.throw_exceptions = rethrow

        # initialize to empty
        self.ps_refresh_token = None
        self.ps_access_token = None
        self.ps_token_exp = None
        self.console = None
        self.recdef_table = {}
        self.arrow_file_table = {} # for processing arrowrec records
        self.local_dns = {} # global dictionary of DNS entries to override

        # configure logging
        self.set_verbose(verbose, loglevel)
        if log_handler != None:
            logger.addHandler(log_handler)

        # configure domain
        self.service_domain = domain

        # configure callbacks
        self.callbacks = {
            'eventrec': Session.__logrec,
            'exceptrec': Session.__alertrec,
            'arrowrec.meta': Session.__arrowrec,
            'arrowrec.data': Session.__arrowrec,
            'arrowrec.eof': Session.__arrowrec
        }

        # configure credentials (if any) for organization
        if organization == 0:
            organization = self.PUBLIC_ORG
        self.authenticate(organization)

        # set cluster to desired number of nodes (if permitted based on credentials)
        self.scaleout(desired_nodes, time_to_live, bypass_dns)

    #
    #  source
    #
    def source (self, api, parm={}, stream=False, callbacks={}, path="/source", force_throw=False):
        '''
        handles making the HTTP request to the sliderule cluster nodes
        '''
        # initialize local variables
        rsps = {}
        headers = {'x-sliderule-client': f'python-{version.full_version}'}

        # build callbacks
        for c in self.callbacks:
            if c not in callbacks:
                callbacks[c] = self.callbacks[c]

        # Construct Request URL
        if self.service_org:
            url = 'https://%s.%s%s/%s' % (self.service_org, self.service_domain, path, api)
        else:
            url = 'http://%s%s/%s' % (self.service_domain, path, api)

        # Construct Payload
        if type(parm) == dict:
            payload = json.dumps(parm)
        else:
            payload = parm

        # Attempt request
        complete = False
        attempts = 3
        while not complete and attempts > 0:
            attempts -= 1
            try:
                # Build Authorization Header
                if self.service_org:
                    self.__buildauthheader(headers)

                # Perform Request
                if not stream:
                    data = self.session.get(url, data=payload, headers=headers, timeout=self.rqst_timeout, verify=self.ssl_verify)
                else:
                    data = self.session.post(url, data=payload, headers=headers, timeout=self.rqst_timeout, stream=True, verify=self.ssl_verify)
                data.raise_for_status()

                # Parse Response
                stream = self.__StreamSource(data)
                format = data.headers['Content-Type']
                if format == 'text/plain':
                    rsps = self.__parse_json(stream)
                elif format == 'application/json':
                    rsps = self.__parse_json(stream)
                elif format == 'application/octet-stream':
                    rsps = self.__parse_native(stream, callbacks)
                elif self.throw_exceptions or force_throw:
                    raise FatalError(f'Unsupported content type: {format}')
                else:
                    logger.error(f'Unsupported content type: {format}')

                # Success
                complete = True

            except requests.exceptions.SSLError as e:
                if self.throw_exceptions or force_throw:
                    raise FatalError(f'Exception in request to {url}: {e}')
                logger.error(f'Unable to verify SSL certificate for {url} ...retrying request')

            except requests.ConnectionError as e:
                if self.throw_exceptions or force_throw:
                    raise FatalError(f'Exception in request to {url}: {e}')
                logger.error(f'Connection error to endpoint {url} ...retrying request')

            except requests.Timeout as e:
                if self.throw_exceptions or force_throw:
                    raise FatalError(f'Exception in request to {url}: {e}')
                logger.error(f'Timed-out waiting for response from endpoint {url} ...retrying request')

            except requests.exceptions.ChunkedEncodingError as e:
                if self.throw_exceptions or force_throw:
                    raise FatalError(f'Exception in request to {url}: {e}')
                logger.error(f'Unexpected termination of response from endpoint {url} ...retrying request')

            except requests.HTTPError as e:
                if self.throw_exceptions or force_throw:
                    if e.response.status_code == 503:
                        raise FatalError(f'Server experiencing heavy load, stalling on request to {url}')
                    else:
                        raise FatalError(f'Exception in request to {url}: {e}')
                logger.error(f'HTTP error <{e.response.status_code}> returned in request to {url} ...retrying request')

        # Check Complete
        if not complete:
            if self.throw_exceptions or force_throw:
                raise FatalError(f'Request to {url} did not complete')
            else:
                rsps = None

        # Return Response
        return rsps

    #
    # get_definition
    #
    def get_definition (self, rectype, fieldname):
        '''
        returns the necessary parsing information for a field in a record
        '''
        # get record definition
        recdef = self.__populate(rectype)

        # return type information for field
        if fieldname in recdef and recdef[fieldname]["type"] in BASIC_TYPES:
            return BASIC_TYPES[recdef[fieldname]["type"]]
        else:
            return {}

    #
    #  set_verbose
    #
    def set_verbose (self, enable, loglevel=logging.INFO):
        '''
        set verbosity of log messages by adding/removing the console log
        handler and by setting the log level
        '''
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
        if (enable == True) and (self.console == None):
            self.console = logging.StreamHandler()
            logger.addHandler(self.console)
        elif (enable == False) and (self.console != None):
            logger.removeHandler(self.console)
            self.console = None

        # always set level to requested
        logger.setLevel(loglevel)
        if self.console != None:
            self.console.setLevel(loglevel)

    #
    # update_available_servers
    #
    def update_available_servers (self, desired_nodes=None, time_to_live=None):
        '''
        makes a capacity request to the provisioning system (if requested) and
        returns the current capacity
        '''
        # initialize local variables
        requested_nodes = 0

        # update number of nodes
        if type(desired_nodes) == int:
            headers = {}
            rsps_body = {}
            requested_nodes = desired_nodes
            self.__buildauthheader(headers)

            # get boundaries of cluster and calculate nodes to request
            try:
                host = "https://ps." + self.service_domain + "/api/org_num_nodes/" + self.service_org + "/"
                rsps = self.session.get(host, headers=headers, timeout=self.rqst_timeout)
                rsps_body = rsps.json()
                rsps.raise_for_status()
                requested_nodes = max(min(desired_nodes, rsps_body["max_nodes"]), rsps_body["min_nodes"])
                if requested_nodes != desired_nodes:
                    logger.info("Provisioning system desired nodes overridden to {}".format(requested_nodes))
            except requests.exceptions.HTTPError as e:
                logger.info('{}'.format(e))
                logger.info('Provisioning system status request returned error => {}'.format(rsps_body["error_msg"]))
                if self.throw_exceptions:
                    raise

            # Request number of nodes in cluster
            try:
                if type(time_to_live) == int:
                    host = "https://ps." + self.service_domain + "/api/desired_org_num_nodes_ttl/" + self.service_org + "/" + str(requested_nodes) + "/" + str(time_to_live) + "/"
                    rsps = self.session.post(host, headers=headers, timeout=self.rqst_timeout)
                else:
                    host = "https://ps." + self.service_domain + "/api/desired_org_num_nodes/" + self.service_org + "/" + str(requested_nodes) + "/"
                    rsps = self.session.put(host, headers=headers, timeout=self.rqst_timeout)
                rsps_body = rsps.json()
                rsps.raise_for_status()
            except requests.exceptions.HTTPError as e:
                logger.info('{}'.format(e))
                logger.error('Provisioning system update request error => {}'.format(rsps_body["error_msg"]))
                if self.throw_exceptions:
                    raise

        # Get number of nodes currently registered
        try:
            rsps = self.source("status", parm={"service":"sliderule"}, path="/discovery", force_throw=True)
            available_servers = rsps["nodes"]
        except FatalError as e:
            logger.debug(f'Failed to retrieve number of nodes registered: {e}')
            available_servers = 0

        return available_servers, requested_nodes

    #
    # scaleout
    #
    def scaleout (self, desired_nodes, time_to_live, bypass_dns):
        '''
        makes a capacity request to the provisioning system and waits
        for the cluster to reach the requested capacity
        '''
        # check desired nodes
        if desired_nodes is None:
            return # nothing needs to be done
        if desired_nodes < 0:
            raise FatalError("Number of desired nodes must be greater than zero ({})".format(desired_nodes))

        # initialize DNS
        self.__initdns() # clear cache of DNS lookups for clusters
        if bypass_dns:
            self.__jamdns() # use ip address for cluster

        # send initial request for desired cluster state
        start = time.time()
        available_nodes,requested_nodes = self.update_available_servers(desired_nodes=desired_nodes, time_to_live=time_to_live)
        scale_up_needed = False

        # Wait for Cluster to Reach Desired State
        while available_nodes < requested_nodes:
            scale_up_needed = True
            logger.info("Waiting while cluster scales to desired capacity (currently at {} nodes, desired is {} nodes)... {} seconds".format(available_nodes, desired_nodes, int(time.time() - start)))
            time.sleep(10.0)
            available_nodes,_ = self.update_available_servers()
            # Override DNS if Cluster is Starting
            if available_nodes == 0 and not self.__dnsoverridden():
                self.__jamdns()
            # Timeout Occurred
            if int(time.time() - start) > self.MAX_PS_CLUSTER_WAIT_SECS:
                logger.error("Maximum time allowed waiting for cluster has been exceeded")
                break

        # Log Final Message if Cluster Needed State Change
        if scale_up_needed:
            logger.info("Cluster has reached capacity of {} nodes... {} seconds".format(available_nodes, int(time.time() - start)))

    #
    # authenticate
    #
    def authenticate (self, ps_organization, ps_username=None, ps_password=None, github_token=None):
        '''
        gets a bearer token (ps_access_token) from the provisioning system to use in requests
        to private clusters
        '''
        # initialize local variables
        login_status = False
        ps_url = "ps." + self.service_domain

        # set organization on any authentication request
        self.service_org = ps_organization

        # check for direct or public access
        if self.service_org == None:
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
                if ps_organization != self.PUBLIC_ORG:
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
                rsps = self.session.post(api, data=json.dumps(rqst), headers=headers, timeout=self.rqst_timeout)
                rsps.raise_for_status()
                rsps = rsps.json()
                self.ps_refresh_token = rsps["refresh"]
                self.ps_access_token = rsps["access"]
                self.ps_token_exp =  time.time() + (float(rsps["access_lifetime"]) / 2)
                login_status = True
            except:
                if ps_organization != self.PUBLIC_ORG:
                    logger.error("Unable to authenticate %s user to %s" % (user, api))

        # return login status
        if ps_organization != self.PUBLIC_ORG:
            logger.info(f'Login status to {self.service_domain}/{self.service_org}: {login_status and "success" or "failure"}')
        return login_status

    #
    #  manager
    #
    def manager (self, api, content_json=True, as_post=False, headers={}):
        '''
        handles making the HTTP request to the sliderule manager
        '''
        # initialize local variables
        rsps = ""

        # Construct Request URL
        if self.service_org:
            url = 'https://%s.%s/manager/%s' % (self.service_org, self.service_domain, api)
        else:
            url = 'http://%s/manager/%s' % (self.service_domain, api)

        try:
            # Build Authorization Header
            if self.service_org:
                self.__buildauthheader(headers)

            # Perform Request
            if as_post:
                data = self.session.post(url, headers=headers, timeout=self.rqst_timeout, verify=self.ssl_verify)
            else:
                data = self.session.get(url, headers=headers, timeout=self.rqst_timeout, verify=self.ssl_verify)
            data.raise_for_status()

            # Parse Response
            stream = self.__StreamSource(data)
            lines = [line for line in stream]
            rsps = b''.join(lines)
            if content_json:
                rsps = json.loads(rsps)

        except requests.exceptions.SSLError as e:
            logger.error(f'Unable to verify SSL certificate for {url}')

        except requests.ConnectionError as e:
            logger.error(f'Connection error to endpoint {url}')

        except requests.Timeout as e:
            logger.error(f'Timed-out waiting for response from endpoint {url}')

        except requests.HTTPError as e:
            logger.error(f'HTTP error <{e.response.status_code}> returned in request to {url}: {rsps}')

        except Exception as e:
            logger.error(f'Failed to make request to {url}: {e}')

        # Return Response
        return rsps

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
    #  __buildauthheader
    #
    def __buildauthheader(self, headers, force_refresh=False):
        '''
        builds the necessary authentication header for the http request to private
        clusters by using the bearer token from the provisioning system
        '''
        if self.ps_access_token:
            # Check if Refresh Needed
            if time.time() > self.ps_token_exp or force_refresh:
                host = "https://ps." + self.service_domain + "/api/org_token/refresh/"
                rqst = {"refresh": self.ps_refresh_token}
                hdrs = {'Content-Type': 'application/json', 'Authorization': 'Bearer ' + self.ps_access_token}
                rsps = self.session.post(host, data=json.dumps(rqst), headers=hdrs, timeout=self.rqst_timeout).json()
                self.ps_refresh_token = rsps["refresh"]
                self.ps_access_token = rsps["access"]
                self.ps_token_exp =  time.time() + (float(rsps["access_lifetime"]) / 2)
            # Build Authentication Header
            headers['Authorization'] = 'Bearer ' + self.ps_access_token

    #
    #  __populate
    #
    def __populate (self, rectype):
        '''
        rectype: record type supplied in response (string)
        '''
        if rectype not in self.recdef_table:
            self.recdef_table[rectype] = self.source("definition", {"rectype" : rectype})
        return self.recdef_table[rectype]

    #
    #  __parse_json
    #
    def __parse_json (self, data):
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
    def __decode_native(self, rectype, rawdata):
        """
        rectype: record type supplied in response (string)
        rawdata: payload supplied in response (byte array)
        """
        # initialize record
        rec = { "__rectype": rectype }

        # get/populate record definition #
        recdef = self.__populate(rectype)

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
            if not self.decode_aux and "AUX" in flags:
                continue

            # get endianness
            if "LE" in flags:
                endian = '<'
            else:
                endian = '>'

            # decode basic type
            if ftype in BASIC_TYPES:

                # check if array
                is_array = not (elems == 1)

                # get number of elements
                if elems <= 0:
                    elems = int((len(rawdata) - offset) / BASIC_TYPES[ftype]["size"])

                # build format string
                fmt = endian + str(elems) + BASIC_TYPES[ftype]["fmt"]

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
                subrecdef = self.__populate(ftype)

                # check if array
                is_array = not (elems == 1)

                # get number of elements
                if elems <= 0:
                    elems = int((len(rawdata) - offset) / subrecdef["__datasize"])

                # return parsed data
                if is_array:
                    rec[fieldname] = []
                    for e in range(elems):
                        rec[fieldname].append(self.__decode_native(ftype, rawdata[offset:]))
                        offset += subrecdef["__datasize"]
                else:
                    rec[fieldname] = self.__decode_native(ftype, rawdata[offset:])

        # return record #
        return rec

    #
    #  __parse_native
    #
    def __parse_native (self, data, callbacks):
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

        for line in data:

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
                        rec     = self.__decode_native(rectype, rawdata)
                        if rectype == "conrec":
                            # parse records contained in record
                            buffer = self.__BufferSource(rawdata[rec["start"]:])
                            contained_recs = self.__parse_native(buffer, callbacks)
                            recs += contained_recs
                        else:
                            if callbacks != None and rectype in callbacks:
                                # Execute Call-Back on Record
                                callbacks[rectype](rec, self)
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

        return recs

    #
    # __initdns
    #
    def __initdns (self):
        '''
        resets the global dictionary
        '''
        self.local_dns.clear()

    #
    # __dnsoverridden
    #
    def __dnsoverridden (self):
        '''
        checks if url is already overridden
        '''
        url = self.service_org + "." + self.service_domain
        return url.lower() in self.local_dns

    #
    # __jamdns
    #
    def __jamdns (self):
        '''
        override the dns entry
        '''
        global sliderule_dns
        headers = {}
        url = self.service_org + "." + self.service_domain
        self.__buildauthheader(headers)
        host = "https://ps." + self.service_domain + "/api/org_ip_adr/" + self.service_org + "/"
        rsps = self.session.get(host, headers=headers, timeout=self.rqst_timeout).json()
        if rsps["status"] == "SUCCESS":
            ipaddr = rsps["ip_address"]
            self.local_dns[url.lower()] = ipaddr
            logger.debug("Overriding DNS for {} with {}".format(url, ipaddr))
        sliderule_dns = self.local_dns

    #
    #  __logrec
    #
    @staticmethod
    def __logrec (rec, session):
        eventlogger[rec['level']]('%d:%s: %s' % (rec["time"], rec['source'], rec["message"]))

    #
    #  __alertrec
    #
    @staticmethod
    def __alertrec (rec, session):
        if rec["code"] == EXCEPTION_CODES["SIMPLIFY"]:
            raise RetryRequest("cmr simplification requested")
        elif rec["code"] < 0:
            eventlogger[rec["level"]]("Alert <%d>: %s", rec["code"], rec["text"])
        else:
            eventlogger[rec["level"]]("%s", rec["text"])

    #
    #  _arrowrec
    #
    @staticmethod
    def __arrowrec (rec, session):
        try :
            filename = rec["filename"]
            if rec["__rectype"] == 'arrowrec.meta':
                if filename in session.arrow_file_table:
                    raise FatalError(f'File transfer already in progress for {filename}')
                logger.info(f'Writing arrow file: {filename}')
                session.arrow_file_table[filename] = { "fp": open(filename, "wb"), "size": rec["size"], "progress": 0 }
            elif rec["__rectype"] == 'arrowrec.eof':
                logger.info(f'Checksum of arrow file: {rec["checksum"]}')
            else: # rec["__rectype"] == 'arrowrec.data'
                data = rec['data']
                file = session.arrow_file_table[filename]
                file["fp"].write(bytearray(data))
                file["progress"] += len(data)
                if file["progress"] >= file["size"]:
                    file["fp"].close()
                    logger.info(f'Closing arrow file: {filename}')
                    del session.arrow_file_table[filename]
        except Exception as e:
            raise FatalError(f'Failed to process arrow file: {e}')
