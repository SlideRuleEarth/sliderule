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

//
// Record Definitions
//
let recordDefinitions = {}

//
// SlideRule Constants
//
const INT8            = 0;
const INT16           = 1;
const INT32           = 2;
const INT64           = 3;
const UINT8           = 4;
const UINT16          = 5;
const UINT32          = 6;
const UINT64          = 7;
const BITFIELD        = 8;
const FLOAT           = 9;
const DOUBLE          = 10;
const TIME8           = 11;
const STRING          = 12;
const USER            = 13;

//
// SlideRule Field Types
//
const fieldtypes = {
  INT8:     {code: INT8,      size: 1},
  INT16:    {code: INT16,     size: 2},
  INT32:    {code: INT32,     size: 4},
  INT64:    {code: INT64,     size: 8},
  UINT8:    {code: UINT8,     size: 1},
  UINT16:   {code: UINT16,    size: 2},
  UINT32:   {code: UINT32,    size: 4},
  UINT64:   {code: UINT64,    size: 8},
  BITFIELD: {code: BITFIELD,  size: 1},
  FLOAT:    {code: FLOAT,     size: 4},
  DOUBLE:   {code: DOUBLE,    size: 8},
  TIME8:    {code: TIME8,     size: 8},
  STRING:   {code: STRING,    size: 1},
  USER:     {code: USER,      size: 0},
};

//------------------------------------
// Local Functions
//------------------------------------

//
// populateDefinition
//
function populateDefinition(rec_type) {
  if (rec_type in recordDefinitions) {
    return recordDefinitions[rec_type];
  }
  else {
    return new Promise((resolve, reject) => {
      exports.source("definition", {"rectype" : rec_type}).then(
        result => {
          recordDefinitions[rec_type] = result;
          resolve(recordDefinitions[rec_type]);
        },
        error => {
          reject(new Error(`failed to retrieve definition for ${rec_type}: ${error}`));
        }
      );
    });
  }
}

//
// getFieldSize
//
async function getFieldSize(type) {
  if (type in fieldtypes) {
    return fieldtypes[type].size;
  }
  else {
    let rec_def = await populateDefinition(type);
    return rec_def.__datasize;
  }
}

//
// decodeElement
//
function decodeElement(type_code, big_endian, buffer, byte_offset) {
  if (big_endian) {
    switch (type_code) {
      case INT8:      return buffer.readInt8(byte_offset);
      case INT16:     return buffer.readInt16BE(byte_offset);
      case INT32:     return buffer.readInt32BE(byte_offset);
      case INT64:     return buffer.readBigInt64BE(byte_offset);
      case UINT8:     return buffer.readUInt8(byte_offset);
      case UINT16:    return buffer.readUInt16BE(byte_offset);
      case UINT32:    return buffer.readUInt32BE(byte_offset);
      case UINT64:    return buffer.readBigUInt64BE(byte_offset);
      case BITFIELD:  throw new Error(`Bit fields are unsupported`);
      case FLOAT:     return buffer.readFloatBE(byte_offset);
      case DOUBLE:    return buffer.readDoubleBE(byte_offset);
      case TIME8:     return new Date(Number(buffer.readBigInt64BE(byte_offset) / BigInt(1000000)));
      case STRING:    return String.fromCharCode(buffer.readUInt8(byte_offset));
      case USER:      throw new Error(`User fields cannot be decoded as a primitive`);
      default:        throw new Error(`Invalid field type ${type_code}`);
    }
  }
  else {
    switch (type_code) {
      case INT8:      return buffer.readInt8(byte_offset);
      case INT16:     return buffer.readInt16LE(byte_offset);
      case INT32:     return buffer.readInt32LE(byte_offset);
      case INT64:     return buffer.readBigInt64LE(byte_offset);
      case UINT8:     return buffer.readUInt8(byte_offset);
      case UINT16:    return buffer.readUInt16LE(byte_offset);
      case UINT32:    return buffer.readUInt32LE(byte_offset);
      case UINT64:    return buffer.readBigUInt64LE(byte_offset);
      case BITFIELD:  throw new Error(`Bit fields are unsupported`);
      case FLOAT:     return buffer.readFloatLE(byte_offset);
      case DOUBLE:    return buffer.readDoubleLE(byte_offset);
      case TIME8:     return new Date(Number(buffer.readBigInt64LE(byte_offset) / BigInt(1000000)));
      case STRING:    return String.fromCharCode(buffer.readUInt8(byte_offset));
      case USER:      throw new Error(`User fields cannot be decoded as a primitive`);
      default:        throw new Error(`Invalid field type ${type_code}`);
    };
  }
}

//
// decodeField
//
async function decodeField(field_def, buffer, offset, rec_size) {
  let value = []; // ultimately returned

  // Pull out field attributes
  let big_endian = (field_def.flags.match('BE') != null);
  let byte_offset = offset + (field_def.offset / 8);
  let num_elements = field_def.elements;
  let field_size = await getFieldSize(field_def.type);

  // Get type code
  let type_code = USER;
  if (field_def.type in fieldtypes) {
     type_code = fieldtypes[field_def.type].code;
  }

  // For variable length fields, recalculate number of elements using size of record
  if (num_elements == 0) {
    num_elements = (rec_size - byte_offset) / field_size;
  }

  // Decode elements
  for (let i = 0; i < num_elements; i++) {
    if (field_def.type in fieldtypes) {
      value.push(decodeElement(type_code, big_endian, buffer, byte_offset));
    }
    else {
      value.push(await decodeRecord(field_def.type, buffer, byte_offset, rec_size));
    }
    byte_offset += field_size;
  }

  // Create final value
  if (type_code == STRING) {
    value = value.join('');
    let null_index = value.indexOf('\0');
    if (null_index > -1) {
      value = value.substr(0, null_index);
    }
  }
  else if (num_elements == 1) {
    value = value[0];
  }

  // Return value
  return value;
}

//
// decodeRecord
//
async function decodeRecord(rec_type, buffer, offset, rec_size) {
  let rec_obj = {}
  let rec_def = await populateDefinition(rec_type);
  // For each field defined in record
  for (let field in rec_def) {
    // Check if not property
    if (field.match(/^__/) == null) {
      rec_obj[field] = await decodeField(rec_def[field], buffer, offset, rec_size);
    }
  }
  // Return decoded record
  return rec_obj;
}

//
// parseResponse
//
function parseResponse (response, resolve, reject, callbacks) {
  // Check Response Code
  if (response.statusCode !== 200) {
    response.resume();
    reject(new Error(`server returned ${response.statusCode}`));
  }
  // Handle Normal Response
  else if (response.headers['content-type'] == 'application/json' ||
           response.headers['content-type'] == 'text/plain') {
    let chunks = [];
    response.on('data', (chunk) => {
      chunks.push(chunk);
    }).on('close', () => {
      let buffer = Buffer.concat(chunks);
      let rsps = JSON.parse(buffer);
      resolve(rsps);
    });
  }
  // Handle Streaming Response
  else if (response.headers['content-type'] == 'application/octet-stream') {
    const REC_HDR_SIZE = 8;
    const REC_VERSION = 2;
    let results = {};
    let chunks = [];
    let total_bytes_read = null;
    let bytes_read = 0;
    let bytes_processed = 0;
    let bytes_to_process = 0;
    let got_header = false;
    let rec_size = 0;
    let rec_type_size = 0;
    response.on('data', (chunk) => {
      chunks.push(chunk);
      bytes_read += chunk.length;
      bytes_to_process += chunk.length;
      while (bytes_to_process > 0) {
        // State: Accumulating Header
        if (!got_header && bytes_to_process > REC_HDR_SIZE) {
          // Process header
          got_header = true;
          bytes_processed += REC_HDR_SIZE;
          bytes_to_process -= REC_HDR_SIZE;
          let buffer = Buffer.concat(chunks);
          // Get header info
          let rec_version = buffer.readUInt16BE(0);
          rec_type_size = buffer.readUInt16BE(2);
          let rec_data_size = buffer.readUInt32BE(4);
          if (rec_version != REC_VERSION) {
            reject(new Error(`invalid record format: ${rec_version}`))
          }
          // Set record attributes
          rec_size = rec_type_size + rec_data_size;
          chunks = [buffer.subarray(REC_HDR_SIZE)];
        }
        // State: Accumulating Record
        else if (got_header && bytes_to_process >= rec_size) {
          // Process record
          got_header = false;
          bytes_to_process -= rec_size;
          bytes_processed += rec_size;
          let buffer = Buffer.concat(chunks);
          let rec_type = buffer.toString('utf8', 0, rec_type_size - 1);
          decodeRecord(rec_type, buffer, rec_type_size, rec_size).then(
            result => {
              if (rec_type in callbacks) {
                callbacks[rec_type](result);
              }
            }
          );
          // Update stats
          if (!(rec_type in results)) {
            results[rec_type] = 0;
          }
          results[rec_type]++;
          // Check if complete
          if ((total_bytes_read != null) && (bytes_processed == total_bytes_read)) {
            results["bytes_processed"] = bytes_processed;
            resolve(results);
          }
          // Restore unused bytes that have been read
          if(bytes_to_process > 0) {
            chunks = [buffer.subarray(rec_size)];
          }
          else {
            chunks = [];
          }
        }
        // State: Need More Data
        else {
          break;
        }
      }
    }).on('close', () => {
      // Establish total bytes read
      total_bytes_read = bytes_read;
      results["bytes_read"] = total_bytes_read;
      // Check if complete
      if (bytes_processed == total_bytes_read) {
        results["bytes_processed"] = bytes_processed;
        resolve(results);
      }
    });
  }
}

//
// httpRequest
//
async function httpRequest(options, body, callbacks) {
  let attempts = 3;
  while (--attempts > 0) {
    let response = new Promise((resolve, reject) => {
      // On Response
      let request = sysConfig.protocol.request(options, (response) =>
        parseResponse(response, resolve, reject, callbacks)
      );
      // On Errors
      request.on('error', (err) => {
        reject(new Error(`failed to make request: ${err.message}`));
      });
      // Populate Body of Request (if provided)
      if (body != null) {
        request.write(body);
      }
      // Terminate Request
      request.end();
    });
    try {
      return await response;
    }
    catch (error) {
      console.log(`retrying http request: ${error}`);
    }
  }
};

//------------------------------------
// Exported Functions
//------------------------------------

//
// Initialize Client
//
exports.init = async (config) => {
  sysConfig = Object.assign(sysConfig, config);
  await exports.authenticate(config.ps_password, config.ps_username);
}

//
// Source Endpoint
//
exports.source = async (api, parm=null, stream=false, callbacks={}) => {

  // Setup Request Options
  const options = {
    host: sysConfig.organization && (sysConfig.organization + '.' + sysConfig.domain) || sysConfig.domain,
    path: '/source/' + api,
    method: stream && 'POST' || 'GET',
    headers: {}
  };

  // Build Body
  let body = null;
  if (parm != null) {
    body = JSON.stringify(parm);
    options['headers']['Content-Type'] = 'application/json';
    options['headers']['Content-Length'] = body.length;
  }

  // Add Authentication
  options["headers"]['Authorization'] = 'Bearer ' + sysCredentials.access;

  // Make API Request
  return httpRequest(options, body, callbacks);
}

//
// Authenticate User
//
exports.authenticate = async (ps_username=null, ps_password=null) => {

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
    return httpRequest(options, body).then(
      result => {
        let expiration = 0;
        try {
          sysCredentials.access = result.access;
          sysCredentials.refresh = result.refresh;
          sysCredentials.expiration =  (Date.now() / 1000) + (result.access_lifetime / 2);
          expiration = sysCredentials.expiration;
        }
        catch (e) {
          console.error("Error processing authentication response\n", result);
        }
        return expiration;
      }
    );
}

//
// Get Version
//
exports.get_version = async () => {
  return exports.source('version').then(
    result => {
      result['client'] = {version: pkg['version']};
      result['organization'] = sysConfig.organization;
      return result;
    }
  );
}

//
// Get Values
//
exports.get_values = (bytearray, fieldtype) => {
  let values = [];
  let buffer = Buffer.from(bytearray);
  switch (fieldtype) {
    case INT8:    for (let i = 0; i < buffer.length; i += 1) values.push(buffer.readInt8LE(i));   break;
    case INT16:   for (let i = 0; i < buffer.length; i += 2) values.push(buffer.readInt16LE(i));  break;
    case INT32:   for (let i = 0; i < buffer.length; i += 4) values.push(buffer.readInt32LE(i));  break;
    case INT64:   for (let i = 0; i < buffer.length; i += 8) values.push(buffer.readInt64LE(i));  break;
    case UINT8:   for (let i = 0; i < buffer.length; i += 1) values.push(buffer.readUInt8LE(i));  break;
    case UINT16:  for (let i = 0; i < buffer.length; i += 2) values.push(buffer.readUInt16LE(i)); break;
    case UINT32:  for (let i = 0; i < buffer.length; i += 4) values.push(buffer.readUInt32LE(i)); break;
    case UINT64:  for (let i = 0; i < buffer.length; i += 8) values.push(buffer.readUInt64LE(i)); break;
    case FLOAT:   for (let i = 0; i < buffer.length; i += 4) values.push(buffer.readFloatLE(i));  break;
    case DOUBLE:  for (let i = 0; i < buffer.length; i += 8) values.push(buffer.readDoubleLE(i)); break;
    case TIME8:   for (let i = 0; i < buffer.length; i += 8) values.push(buffer.readInt64LE(i));  break;
    case STRING:  values = String.fromCharCode.apply(null, bytearray); break;
    default: break;
  }
  return values;
}