// Copyright (c) 2021, University of Washington
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the University of Washington nor the names of its
//    contributors may be used to endorse or promote products derived from this
//    software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
// “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

const https = require('https');
const netrc = require('netrc');
const pkg = require('./package.json')

//------------------------------------
// File Data
//------------------------------------

//
// System Credentials
//
let sysCredentials = {
  refresh: null,
  access: null,
  expiration: 0,
};

//
// System Configuration
//
let sysConfig = {
  domain: "slideruleearth.io",
  organization: "sliderule",
  protocol: https,
  verbose: true,
  desired_nodes: null,
  time_to_live: 60,
  timeout: 120000, // milliseconds
};

//------------------------------------
// Local Functions
//------------------------------------

//
// httpRequest
//
function httpRequest(options, body, onClose) {
  let promise = new Promise((resolve, reject) => {
    let request = sysConfig.protocol.request(options, (res) => {
      if (res.statusCode !== 200) {
        res.resume();
        reject(new Error(`server returned ${res.statusCode}`));
      }

      let data = '';

      res.on('data', (chunk) => {
        data += chunk;
      });

      res.on('close', () => {
        resolve(onClose(data));
      });
    });

    request.on('error', (err) => {
      reject(new Error(err.message));
    });

    if (body != null) {
      request.write(JSON.stringify(body));
    }

    request.end();
  });

  return promise;
};

//------------------------------------
// Exported Functions
//------------------------------------

//
// Initialize Client
//
exports.init = (config) => {
  sysConfig = Object.assign(sysConfig, config)
}

//
// Source Endpoint
//
exports.source = (api, parm=null, stream=false) => {

  // Setup Request Options
  const options = {
    host: sysConfig.organization && (sysConfig.organization + '.' + sysConfig.domain) || sysConfig.domain,
    path: '/source/' + api,
    method: stream && 'POST' || 'GET',
  };

  // Make API Request
  return httpRequest(options, parm, (data) => JSON.parse(data));
}

//
// Authenticate User
//
exports.authenticate = (ps_username=null, ps_password=null) => {

    // Build Provisioning System URL
    let psHost = 'ps.' + sysConfig.domain;

    // Obtain Username and Password
    ps_username = ps_username ?? process.env.PS_USERNAME;
    ps_password = ps_password ?? process.env.PS_PASSWORD;
    if (ps_username == null || ps_password == null) {
      let myNetrc = netrc();
      if (psHost in myNetrc) {
        ps_username = myNetrc[psHost].login;
        ps_password = myNetrc[psHost].password;
      }
    }

    // Bail If Username and Password Not Found
    if(!ps_username && !ps_password) {
      console.error(`Unable to obtain username and/or password for ${psHost}`);
      return Promise.resolve();
    }

    // Build Request Body
    let body = JSON.stringify({username: ps_username, password: ps_password, org_name: sysConfig.organization});

    // Setup Request Options
    const options = {
      host: psHost,
      path: '/api/org_token/',
      method: 'POST',
      headers: {'Content-Type': 'application/json', 'Content-Length': body.length},
    };

    // Make Authentication Request
    return httpRequest(options, body, (data) => {
      let expiration = 0;
      try {
        rsps = JSON.parse(data);
        sysCredentials.access = rsps.access;
        sysCredentials.refresh = rsps.refresh;
        sysCredentials.expiration =  (Date.now() / 1000) + (rsps.access_lifetime / 2);
        expiration = sysCredentials.expiration;
      }
      catch (e) {
        console.error("Error processing authentication response\n", data);
      }
      return expiration;
    });
}

//
// Get Version
//
exports.get_version = () => {
  return exports.source('version').then(
    result => {
      result['client'] = {version: pkg['version']};
      result['organization'] = sysConfig.organization;
      return result;
    }
  );
}