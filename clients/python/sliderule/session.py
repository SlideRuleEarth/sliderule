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
import sys
import requests
import threading
import json
import struct
import ctypes
import time
import logging
import numpy
import base64
from pathlib import Path
from datetime import datetime, timezone
from urllib import parse as urllib_parse
import webbrowser
from sliderule import version

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

    PUBLIC_DOMAIN = "slideruleearth.io"
    PUBLIC_CLUSTER = "sliderule"
    PRIVATE_KEY = ".sliderule_key"
    MAX_PS_CLUSTER_WAIT_SECS = 600

    #
    # constructor
    #
    def __init__ (self,
        domain          = PUBLIC_DOMAIN,
        cluster         = PUBLIC_CLUSTER,
        node_capacity   = None,
        ttl             = 60, # minutes
        verbose         = False,
        loglevel        = logging.INFO,
        trust_env       = False,
        ssl_verify      = True,
        rqst_timeout    = (10, 120), # (connection, read) in seconds
        decode_aux      = True,
        rethrow         = False,
        github_token    = None,
        private_key     = PRIVATE_KEY,
        user_service    = False):
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
        self.domain = domain
        self.cluster = cluster
        self.private_key = private_key

        # initialize to empty
        self.ps_access_token = None
        self.ps_token_exp = None
        self.ps_metadata = None
        self.ps_lock = threading.Lock()
        self.recdef_table = {}
        self.arrow_file_table = {} # for processing arrowrec records

        # initialize logging
        self.logger = logging.getLogger(__name__)
        self.eventlogger = {
            0: self.logger.debug,
            1: self.logger.info,
            2: self.logger.warning,
            3: self.logger.error,
            4: self.logger.critical
        }

        # set verbosity
        if verbose:
            self.set_verbose(loglevel)

        # configure callbacks
        self.callbacks = {
            'eventrec': Session.__logrec,
            'exceptrec': Session.__alertrec,
            'arrowrec.meta': Session.__arrowrec,
            'arrowrec.data': Session.__arrowrec,
            'arrowrec.eof': Session.__arrowrec
        }

        # authenticate for non-public clusters
        if ((self.cluster != self.PUBLIC_CLUSTER) and (self.cluster != None)) or user_service:
            self.authenticate(github_token=github_token)

        # set service
        if user_service:
            self.service = self.ps_metadata['sub']
        else:
            self.service = self.cluster

        # create wrappers for gateway applications
        self.authenticator = self.__Authenticator(self)
        self.provisioner = self.__Provisioner(self)
        self.runner = self.__Runner(self)

        # auto-deploy when node capacity provided
        if isinstance(node_capacity, int) and node_capacity > 0:
            self.scaleout(node_capacity, ttl)

    #
    #  source
    #
    def source (self, api, parm=None, stream=False, callbacks=None, path="/source", retries=2, rethrow=False, sign=False, headers=None):
        '''
        handles making the HTTP request to the sliderule cluster nodes
        '''
        # initialize parameters
        if not callbacks: callbacks = {}
        if not headers: headers = {}

        # initialize response
        rsps = {}

        # initialize headers
        headers = headers | {'x-sliderule-client': f'python-{version.full_version}'}

        # set service header (if applicable)
        if self.service != self.cluster:
            headers = headers | {'x-sliderule-service': self.service}

        # build callbacks
        for c in self.callbacks:
            if c not in callbacks:
                callbacks[c] = self.callbacks[c]

        # Construct Request URL
        if self.cluster:
            url = f'https://{self.cluster}.{self.domain}{path}/{api}'
        else:
            url = f'http://{self.domain}{path}/{api}'

        # Construct Payload
        if isinstance(parm, dict):
            payload = json.dumps(parm)
        elif parm is not None:
            payload = parm
        else:
            payload = ''

        # (Optionally) Sign Request
        if sign:
            self.__signrequest(headers, url.split("://")[-1], payload)

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
                self.logger.debug(f'{rsps}... {retry_status(remaining_attempts)}')

        # Check Complete
        if not complete:
            if self.throw_exceptions or rethrow:
                raise FatalError(f'error in request to {url}: {rsps}')
            else:
                self.logger.error(f'Error in request to {url}: {rsps}')
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
    def set_verbose (self, loglevel=logging.INFO):
        '''
        set verbosity of log messages by adding the console log
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

        # enable logging to console
        if len(self.logger.handlers) == 0:
            self.logger.addHandler(logging.StreamHandler())

        # always set level to requested
        self.logger.setLevel(loglevel)
        for handler in self.logger.handlers:
            handler.setLevel(loglevel)
    #
    # scaleout
    #
    def scaleout (self, node_capacity, ttl, version="latest", is_public=False, block=True):
        '''
        makes a deployment request to the SlideRule Provisioner and (optional, by default) waits for the cluster to reach the requested capacity
        '''
        # Initial Conditions
        available_nodes = 0
        inerror = False
        deploy_needed = isinstance(node_capacity, int) and (node_capacity > 0) and isinstance(ttl, int) and (ttl > 0)
        start = time.time()

        # Wait for Cluster to Reach Desired State
        while True:

            # Get Current Cluster Node Capacity
            try:
                rsps = self.source("status", parm={"service":self.service}, path="/discovery", retries=0, rethrow=True)
                available_nodes = rsps["nodes"]
                self.logger.debug(f'Discovery response for {self.service}: {json.dumps(rsps)}')
            except RuntimeError as e:
                self.logger.debug(f'Failed to discover available nodes: {e}')
                available_nodes = 0

            # Check if Deployment Needed
            if deploy_needed and available_nodes < node_capacity:
                deploy_needed = False

                # Deployment Request
                rsps = self.provisioner.deploy(is_public=is_public, node_capacity=node_capacity, ttl=ttl, version=version)
                inerror = "error" in rsps
                self.logger.info(f'Requesting deployment of {self.cluster}: {"failed" if inerror else "succeeded"}')
                if inerror: # error occurred
                    self.logger.critical(f'Unable to deploy {self.cluster}: {rsps.get("exception")}')
                    break

            # Check for Exit Conditions
            if not block:
                break
            elif int(time.time() - start) > self.MAX_PS_CLUSTER_WAIT_SECS: # Timeout
                self.logger.error("Maximum time allowed waiting for cluster has been exceeded")
                break
            elif available_nodes >= node_capacity: # Node Capacity Reached
                self.logger.info(f"Cluster has reached capacity of {available_nodes} nodes... {int(time.time() - start)} seconds")
                break
            else:
                self.logger.info(f"Waiting while cluster scales to desired capacity (currently at {available_nodes} nodes, desired is {node_capacity} nodes)... {int(time.time() - start)} seconds")
                time.sleep(10.0)

        # return status
        return available_nodes, node_capacity or 0

    #
    # authenticate
    #
    def authenticate (self, github_token=None, force_login=False):
        '''
        gets a bearer token (ps_access_token) from sliderule
        to use in requests to private clusters
        '''
        # initialize local variables
        login_status = False
        github_user_token = os.environ.get("SLIDERULE_GITHUB_TOKEN", github_token)
        login_url = "https://login." + self.domain

        # authenticate user
        try:
            if github_user_token and not force_login:
                # directly login using a PAT key
                rsps = self.__patlogin(login_url, github_user_token)
            else:
                # attempt device flow login (required for admin privileges)
                rsps = self.__deviceflow(login_url, force_login=force_login)

            # set internal session data
            if rsps.get('status') == 'success':
                now = time.time()
                self.ps_metadata = rsps['metadata']
                self.ps_access_token = rsps['token']
                self.ps_token_exp =  int(((self.ps_metadata['exp'] - now) / 2) + now)
                login_status = True
            else:
                raise RuntimeError(f'{rsps}')

        except Exception as e:
            self.logger.error(f'Failure attempting to authenticate: {e}')

        # log status
        self.logger.info(f'Login status to {login_url}: {login_status and "success" or "failure"}')

        # return response metadata
        return self.ps_metadata

    #
    #  gateway_request
    #
    def gateway_request (self, api, subdomain=None, data=None, headers=None):
        '''
        handles making the HTTP request to a sliderule gateway service
        '''
        if headers == None:
            headers = {}

        try:
            # Initialize Response
            rsps = None

            # Build Authorization Header
            self.__buildauthheader(headers)

            # Perform Request
            path = f'{subdomain}.{self.domain}/{api}'
            url = f'https://{path}'
            body = json.dumps(data)
            self.__signrequest(headers, path, body) # (optionally) sign request
            def do_post():
                return self.session.post(url, data=body, headers=headers, timeout=self.rqst_timeout, verify=self.ssl_verify)
            data = do_post()
            if data.status_code == 401: # AWS API Gateway will often return a 401 on a cold request
                retry_delay = 1 # seconds
                self.logger.info(f"Retrying gateway post after {retry_delay} seconds ...")
                time.sleep(retry_delay)
                data = do_post()

            # Parse Response
            stream_source = self.__StreamSource(data)
            lines = [line for line in stream_source]
            rsps = b''.join(lines)
            rsps = json.loads(rsps)

            # Check for Errors
            data.raise_for_status()

            # Return Response
            return rsps

        except Exception as e:
            self.logger.error(f'Failed to make request to {url}: {e}')
            if self.throw_exceptions:
                raise
            return {'error': f'{e}', 'error_description': f'{rsps}'}

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
    # __Provisioner
    #
    class __Provisioner:
        def __init__ (self, session):
            self.session = session
            self.api_path_suffix = ""
            if self.session.service != self.session.cluster:
                self.api_path_suffix += f"/{self.session.service}"
        def deploy (self, *, is_public, node_capacity, ttl, version):
            return self.session.gateway_request("deploy" + self.api_path_suffix, subdomain="provisioner", data={"cluster": self.session.cluster, "is_public": is_public, "node_capacity": node_capacity, "ttl": ttl, "version": version})
        def extend (self, *, ttl):
            return self.session.gateway_request("extend" + self.api_path_suffix, subdomain="provisioner", data={"cluster": self.session.cluster, "ttl": ttl})
        def destroy (self):
            return self.session.gateway_request("destroy", subdomain="provisioner", data={"cluster": self.session.cluster})
        def status (self):
            return self.session.gateway_request("status" + self.api_path_suffix, subdomain="provisioner", data={"cluster": self.session.cluster})
        def events (self):
            return self.session.gateway_request("events" + self.api_path_suffix, subdomain="provisioner", data={"cluster": self.session.cluster})
        def report (self, kind="clusters"):
            return self.session.gateway_request(f"report/{kind}", subdomain="provisioner", data={})
        def info (self):
            return self.session.gateway_request(f"info", subdomain="provisioner", data={})

    #
    # __Runner
    #
    class __Runner:
        def __init__ (self, session):
            self.session = session
        def submit (self, *, name, script, args_list, optional_args=None):
            if optional_args == None: optional_parms = {}
            return self.session.gateway_request("submit", subdomain="runner", data={"name": name, "script": base64.b64encode(script.encode()).decode(), "args_list": args_list} | optional_args)
        def jobs (self, *, job_list):
            return self.session.gateway_request("report/jobs", subdomain="runner", data={"job_list": job_list})
        def queue (self, *, job_state):
            return self.session.gateway_request("report/queue", subdomain="runner", data={"job_state": job_state})
        def cancel (self, *, job_list):
            return self.session.gateway_request("cancel", subdomain="runner", data={"job_list": job_list})
        def cancel_all (self):
            return self.session.gateway_request("cancel", subdomain="runner")

    #
    # __Authenticator
    #
    class __Authenticator:
        def __init__ (self, session):
            self.session = session
        def info (self):
            def decode_part(part):
                padded = part + "=" * (-len(part) % 4)
                return json.loads(base64.urlsafe_b64decode(padded))
            header_b64, payload_b64, signature_b64 = self.session.ps_access_token.split(".")
            header = decode_part(header_b64)
            claims = decode_part(payload_b64)
            return header, claims

    #
    #  __deviceflow
    #
    def __deviceflow(self, login_url, timeout=60, force_login=False):
        """
        Prompt the user through the device flow authentication to GitHub
        """
        if not force_login:
            # attempt to retrieve token from local cache
            result = self.__load_cache("ps_access_token.json")
            try:
                if result and (result["status"] == "success") and (int(datetime.now().timestamp()) < result["metadata"]["exp"]):
                    return result
                else:
                    ts = datetime.fromtimestamp(result['metadata']['exp'])
                    self.logger.info(f"Invalid or expired token: {ts.strftime('%Y-%m-%d %H:%M:%S')}")
            except Exception as e:
                self.logger.error(f"Exception occurred when reading token from local cache: {e}")

        # sequence user through device flow
        try:
            # get device info from github
            response                    = self.session.post(login_url + '/auth/github/device')
            device_info                 = response.json()
            user_code                   = device_info['user_code']
            device_code                 = device_info['device_code']
            verification_uri            = device_info['verification_uri']
            verification_uri_complete   = device_info['verification_uri_complete'] or f"{verification_uri}?user_code={urllib_parse.quote(user_code)}"
            interval                    = device_info.get('interval', 5) # default to check every 5 seconds
            expires_in                  = device_info.get('expires_in', timeout)

            # display instructions to user
            print("\n" + "=" * 60)
            print("GitHub Authentication Required")
            print("=" * 60)
            print(f"\nPlease visit: {verification_uri}")
            print(f"\nAnd enter this code: {user_code}")
            print("\n" + "=" * 60)

            # open browser if possible
            try:
                print("Attempting to open browser... ", end='')
                webbrowser.open(verification_uri_complete)
                print("success.")
            except Exception:
                print("failed (must manually open).")

            # poll for authorization
            print("Waiting for authorization...  ", end='')
            print_mod = 0
            start_time = time.time()
            actual_timeout = min(timeout, expires_in)
            while time.time() - start_time < actual_timeout:
                # display wait symbol
                spinner = ['/', '-', '\\', '-']
                sys.stdout.write(f"\b{spinner[print_mod]}")
                sys.stdout.flush()
                print_mod = (print_mod + 1) % 4
                # make polling request
                response = self.session.post(login_url + '/auth/github/device/poll', json={'device_code': device_code}, headers={'Content-Type': 'application/json'})
                result = response.json()
                if result['status'] == 'success':
                    print(f"\bsuccess")
                    # attempt to store token to local cache
                    if not self.__store_cache("ps_access_token.json", result):
                        self.logger.info(f"Unsuccessful storing token to local cache")
                    # return metadata
                    return result
                elif result['status'] == 'error':
                    self.logger.error(f'error polling for authorization: {result.get("error")}')
                else: # result['status'] == 'pending'
                    wait_interval = result.get('interval', interval)
                    time.sleep(wait_interval)

            # if here then we failed to authenticate
            print('\bfailure')
            self.logger.error(f'failed to authenticate to {login_url}')

        # handle exceptions
        except requests.RequestException as e:
            self.logger.error(f"request failed: {e}")
        except json.JSONDecodeError:
            self.logger.error(f"invalid json response")
        except Exception as e:
            self.logger.error(f"internal error: {e}")

        # return error
        return {'status': 'error'}

    #
    #  __patlogin
    #
    def __patlogin(self, login_url, pat):
        """
        Login directly with a GitHub Personal Access Token (PAT) key
        """
        try:
            # make login request
            response = self.session.post(login_url + '/auth/github/pat', json={'pat': pat})
            result = response.json()

            # return success
            return result

        # handle exceptions
        except requests.RequestException as e:
            self.logger.error(f"request failed: {e}")
        except json.JSONDecodeError:
            self.logger.error(f"invalid json response")
        except Exception as e:
            self.logger.error(f"internal error: {e}")

        # return error
        return {'status': 'error'}

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
                    host = "https://login." + self.domain + "/auth/refresh/"
                    hdrs = {'Authorization': 'Bearer ' + self.ps_access_token}
                    try:
                        rsps = self.session.get(host, headers=hdrs, timeout=self.rqst_timeout).json()
                        self.ps_access_token = rsps["token"]
                        self.ps_token_exp = int(((rsps['exp'] - now) / 2) + now)
                    except Exception as e:
                        self.logger.error(f'Failed to refresh token: {e}')
            # Build Authentication Header
            headers['Authorization'] = 'Bearer ' + self.ps_access_token

    #
    #  __signrequest
    #
    def __signrequest(self, headers, path, body):
        '''
        cryptographically sign the request
        '''
        try:
            from cryptography.hazmat.primitives.serialization import load_ssh_private_key
            with open(os.path.join(Path.home(), self.private_key), "rb") as file:
                private_key = load_ssh_private_key(file.read(), password=None)
            path_b64 = base64.urlsafe_b64encode(path.encode()).decode()
            timestamp = str(int(datetime.now(timezone.utc).timestamp()))
            body_b64 = base64.urlsafe_b64encode(body.encode()).decode()
            canonical_string = f"{path_b64}:{timestamp}:{body_b64}"
            message_bytes = canonical_string.encode("utf-8")
            signature = private_key.sign(message_bytes)
            signature_b64 = base64.b64encode(signature).decode("ascii")
            headers["x-sliderule-timestamp"] = str(timestamp)
            headers["x-sliderule-signature"] = signature_b64
        except Exception as e:
            self.logger.warning(f"Failed to sign request: {e}")


    #
    #  __load_cache
    #
    def __load_cache (self, filename):
        try:
            cache_file = Path(Path.home() / ".cache" / "sliderule" / filename)
            with open(cache_file, "r") as file:
                return json.loads(file.read())
        except Exception as e:
            self.logger.error(f'Failed load data from {filename}: {e}')
            return None

    #
    #  __store_cache
    #
    def __store_cache (self, filename, contents):
        # retrieve token from local cache
        try:
            cache_dir = Path(Path.home() / ".cache" / "sliderule")
            cache_dir.mkdir(mode=0o700, parents=True, exist_ok=True)
            cache_file = cache_dir / filename
            fd = os.open(cache_file, os.O_WRONLY | os.O_CREAT, mode=0o600)
            with os.fdopen(fd, "w") as file:
                file.write(json.dumps(contents))
            return True
        except Exception as e:
            self.logger.error(f'Failed store data to {filename}: {e}')
            return False

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
        session.eventlogger[rec['level']]('%d:%s: %s' % (rec["time"], rec['source'], rec["message"]))

    #
    #  __alertrec
    #
    @staticmethod
    def __alertrec (rec, session):
        if rec["code"] < 0:
            session.eventlogger[rec["level"]]("Alert <%d>: %s", rec["code"], rec["text"])
        else:
            session.eventlogger[rec["level"]]("%s", rec["text"])

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
                session.logger.info(f'Writing output file: {filename}')
                session.arrow_file_table[filename] = { "fp": open(filename, "wb"), "size": rec["size"], "progress": 0 }
            elif rec["__rectype"] == 'arrowrec.eof':
                session.logger.info(f'Checksum of output file: {rec["checksum"]}')
            else: # rec["__rectype"] == 'arrowrec.data'
                data = rec['data']
                file = session.arrow_file_table[filename]
                file["fp"].write(bytearray(data))
                file["progress"] += len(data)
                if file["progress"] >= file["size"]:
                    file["fp"].close()
                    session.logger.info(f'Closing output file: {filename}')
                    del session.arrow_file_table[filename]
        except Exception as e:
            raise FatalError(f'Failed to process arrow file: {e}')
