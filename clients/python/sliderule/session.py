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
import threading
import json
import struct
import ctypes
import time
import logging
import numpy
import webbrowser
from urllib.parse import urljoin
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
# GLOBALS
###############################################################################

EXCEPTION_CODES = {
    "INFO": 0,
    "ERROR": -1,
    "TIMEOUT": -2,
    "RESOURCE_DOES_NOT_EXIST": -3,
    "EMPTY_SUBSET": -4,
    "SIMPLIFY": -5,
    "NOT_ENOUGH_MEMORY": -6,
    "DOES_NOT_EXIST": -7,
    "UNAUTHORIZED": -8,
    "DID_NOT_COMPLETE": -9,
    "TOO_MANY_RESOURCES": -10
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
        trust_env       = False,
        ssl_verify      = True,
        rqst_timeout    = (10, 120), # (connection, read) in seconds
        log_handler     = None,
        decode_aux      = True,
        rethrow         = False,
        github_token    = None):
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
        self.ps_access_token = None
        self.ps_token_exp = None
        self.ps_lock = threading.Lock()
        self.console = None
        self.recdef_table = {}
        self.arrow_file_table = {} # for processing arrowrec records

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
        if organization != 0:
            self.authenticate(organization, github_token=github_token, verbose=verbose)
            self.scaleout(desired_nodes, time_to_live)
        else:
            organization = self.PUBLIC_ORG

    #
    #  source
    #
    def source (self, api, parm=None, stream=False, callbacks=None, path="/source", retries=2, rethrow=False):
        '''
        handles making the HTTP request to the sliderule cluster nodes
        '''
        # initialize local variables
        rsps = {}
        headers = {'x-sliderule-client': f'python-{version.full_version}'}

        # initialize parameters
        if callbacks == None:
            callbacks = {}

        # build callbacks
        for c in self.callbacks:
            if c not in callbacks:
                callbacks[c] = self.callbacks[c]

        # Construct Request URL
        if self.service_org:
            url = f'https://{self.service_org}.{self.service_domain}{path}/{api}'
        else:
            url = f'http://{self.service_domain}{path}/{api}'

        # Construct Payload
        if isinstance(parm, dict):
            payload = json.dumps(parm)
        else:
            payload = parm

        # status helper function
        def retry_status(attempt):
            return f"attempt {1 + retries - attempt} of {1 + retries} to {url}"

        # Attempt request
        complete = False
        remaining_attempts = 1 + retries
        while not complete and remaining_attempts > 0:
            remaining_attempts -= 1
            try:
                # Build Authorization Header
                if self.service_org:
                    self.__buildauthheader(headers)

                # Perform Request
                if not stream:
                    data = self.session.get(url, data=payload, headers=headers, timeout=self.rqst_timeout, verify=self.ssl_verify)
                else:
                    data = self.session.post(url, data=payload, headers=headers, timeout=self.rqst_timeout, stream=True, verify=self.ssl_verify)

                # Parse Response
                stream_source = self.__StreamSource(data)
                format = data.headers.get('Content-Type')
                if format == 'application/octet-stream':
                    rsps = self.__parse_native(stream_source, callbacks)
                else:
                    rsps = self.__parse_text(stream_source)

                # Handle Error Codes
                data.raise_for_status()

                # Success
                complete = True

            except requests.ConnectionError as e:
                rsps = 'Connection error'

            except requests.Timeout as e:
                rsps = 'Timed-out waiting for response'

            except requests.exceptions.ChunkedEncodingError as e:
                rsps = 'Unexpected termination of response'

            except requests.exceptions.SSLError as e:
                rsps = 'Unable to verify SSL certificate'
                break # skip retries

            except requests.HTTPError as e:
                if e.response.status_code == 503:
                    rsps = 'Cluster experiencing heavy load'
                else:
                    rsps = f'HTTP error <{e.response.status_code}> {rsps}'
                    break # skip retries

            except Exception as e:
                rsps = f'Exception processing request: {e}'
                break # skip retries

            # Log Reason for Not Completing
            if not complete:
                logger.error(f'{rsps}... {retry_status(remaining_attempts)}')

        # Check Complete
        if not complete:
            if self.throw_exceptions or rethrow:
                raise FatalError(f'error in request to {url}: {rsps}')
            else:
                logger.error(f'Error in request to {url}: {rsps}')
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

        # get number of nodes currently registered
        try:
            rsps = self.source("status", parm={"service":"sliderule"}, path="/discovery", retries=0)
            available_servers = rsps["nodes"]
        except FatalError as e:
            logger.debug(f'Failed to retrieve number of nodes registered: {e}')
            available_servers = 0

        # update number of nodes
        if isinstance(desired_nodes, int):
            headers = {}
            rsps_body = {}
            requested_nodes = desired_nodes
            self.__buildauthheader(headers)

            # build request
            url = "https://provisioner." + self.service_domain + "/deploy"
            payload = json.dumps({
                "Cluster": self.service_org,
                "IsPublic": False,
                "NodeCapacity": desired_nodes,
                "TTL": time_to_live
            })

            # make provisioning request
            try:
                rsps = self.session.post(url, data=payload, headers=headers, timeout=self.rqst_timeout)
                rsps_body = rsps.json()
                rsps.raise_for_status()
            except requests.exceptions.HTTPError as e:
                logger.error(f'Provisioning system update request error => {rsps_body.get("Exception")}')
                if self.throw_exceptions:
                    raise
            except Exception as e:
                logger.info(f'Failed to update available servers: {e}')
                if self.throw_exceptions:
                    raise

        # return status
        return available_servers, requested_nodes

    #
    # scaleout
    #
    def scaleout (self, desired_nodes, time_to_live):
        '''
        makes a capacity request to the provisioning system and waits
        for the cluster to reach the requested capacity
        '''
        # check desired nodes
        if desired_nodes is None:
            return # nothing needs to be done
        if desired_nodes < 0:
            raise FatalError("Number of desired nodes must be greater than zero ({})".format(desired_nodes))

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
    def authenticate (self, organization, github_token=None, verbose=False):
        '''
        gets a bearer token (ps_access_token) from sliderule
        to use in requests to private clusters
        '''
        # initialize local variables
        login_status = False
        github_user_token = os.environ.get("SLIDERULE_GITHUB_TOKEN", github_token)
        login_url = "https://login." + self.service_domain
        metadata = {}

        # set organization on any authentication request
        self.service_org = organization

        # check for direct or public access
        if self.service_org == None:
            return True

        # authenticate user
        try:
            if github_user_token:
                # github user tokens currently unsupported
                rsps = {'status': 'unsupported'}
            else:
                # attempt device flow login
                rsps = self.__deviceflow(login_url)

            # set internal session data
            if rsps['status'] == 'success':
                now = time.time()
                metadata = rsps['metadata']
                self.ps_access_token = rsps['token']
                self.ps_token_exp =  int(((metadata['exp'] - now) / 2) + now)
                login_status = True

        except Exception as e:
            logger.error(f'Failure attempting to authenticate: {e}')

        # log status
        if organization != self.PUBLIC_ORG:
            logger.info(f'Login status to {login_url}: {login_status and "success" or "failure"}')
        if verbose:
            logger.info(f'Login metadata\n{json.dumps(metadata, indent=2)}')

        # return response metadata
        return metadata

    #
    #  manager
    #
    def manager (self, api, content_json=True, as_post=False, headers=None):
        '''
        handles making the HTTP request to the sliderule manager
        '''
        # default parameters
        if headers == None:
            headers = {}

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
            stream_source = self.__StreamSource(data)
            lines = [line for line in stream_source]
            rsps = b''.join(lines)
            if content_json:
                rsps = json.loads(rsps)

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
    #  __deviceflow
    #
    def __deviceflow(self, login_url, timeout=60):
        """
        Prompt the user through the device flow authentication to GitHub
        """
        result = {'status': 'error'}
        try:
            # get device info from github
            response            = self.session.post(login_url + '/auth/github/device')
            device_info         = response.json()
            user_code           = device_info['user_code']
            device_code         = device_info['device_code']
            verification_uri    = device_info.get('verification_uri_complete') or device_info['verification_uri']
            interval            = device_info.get('interval', 5) # default to check every 5 seconds
            expires_in          = device_info.get('expires_in', timeout)

            # display instructions to user
            print("\n" + "=" * 60)
            print("GitHub Authentication Required")
            print("=" * 60)
            print(f"\nPlease visit: {verification_uri}")
            print(f"\nAnd enter this code: {user_code}")
            print("\n" + "=" * 60)

            # Open browser if possible
            try:
                print("\nAttempting to open browser... ", end='')
                webbrowser.open(verification_uri)
                print("success.")
            except Exception:
                print("failed (must manually open).")

            # Poll for authorization
            print("\nWaiting for authorization ", end='')
            start_time = time.time()
            actual_timeout = min(timeout, expires_in)
            while time.time() - start_time < actual_timeout:
                print(".", end='')
                response = self.session.post(login_url + '/auth/github/device/poll', json={'device_code': device_code}, headers={'Content-Type': 'application/json'})
                result = response.json()
                if result['status'] == 'success':
                    print(f"success")
                    break
                elif result['status'] == 'error':
                    logger.error(f'error polling for authorization: {result.get('error')}')
                else: # result['status'] == 'pending'
                    wait_interval = result.get('interval', interval)
                    time.sleep(wait_interval)

            # if here then we failed to authenticate
            print('failure')
            logger.error(f'failed to authenticate to {login_url}')

        # handle exceptions
        except requests.RequestException as e:
            logger.error(f"request failed: {e}")
        except json.JSONDecodeError:
            logger.error(f"invalid json response")
        except Exception as e:
            logger.error(f"internal error: {e}")

        # return result (which includes status)
        return result

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
            now = time.time()
            with self.ps_lock:
                if now > self.ps_token_exp or force_refresh:
                    host = "https://login." + self.service_domain + "/auth/refresh/"
                    hdrs = {'Authorization': 'Bearer ' + self.ps_access_token}
                    try:
                        rsps = self.session.get(host, headers=hdrs, timeout=self.rqst_timeout).json()
                        self.ps_access_token = rsps["token"]
                        self.ps_token_exp = int(((rsps['exp'] - now) / 2) + now)
                    except Exception as e:
                        logger.error(f'Failed to refresh token: {e}')
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
    #  __parse_text
    #
    def __parse_text (self, data):
        """
        data: request response
        """
        lines = []
        for line in data:
            lines.append(line)
        response = b''.join(lines)
        try:
            return json.loads(response)
        except Exception:
            return response

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
        if rec["code"] < 0:
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
                logger.info(f'Writing output file: {filename}')
                session.arrow_file_table[filename] = { "fp": open(filename, "wb"), "size": rec["size"], "progress": 0 }
            elif rec["__rectype"] == 'arrowrec.eof':
                logger.info(f'Checksum of output file: {rec["checksum"]}')
            else: # rec["__rectype"] == 'arrowrec.data'
                data = rec['data']
                file = session.arrow_file_table[filename]
                file["fp"].write(bytearray(data))
                file["progress"] += len(data)
                if file["progress"] >= file["size"]:
                    file["fp"].close()
                    logger.info(f'Closing output file: {filename}')
                    del session.arrow_file_table[filename]
        except Exception as e:
            raise FatalError(f'Failed to process arrow file: {e}')
