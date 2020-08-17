/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "EndpointObject.h"
#include "LogLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EndpointObject::OBJECT_TYPE = "EndpointObject";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointObject::EndpointObject (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EndpointObject::~EndpointObject (void)
{
}

/*----------------------------------------------------------------------------
 * str2verb
 *----------------------------------------------------------------------------*/
EndpointObject::verb_t EndpointObject::str2verb (const char* str)
{
         if(StringLib::match(str, "GET"))       return GET;
    else if(StringLib::match(str, "HEAD"))      return HEAD;
    else if(StringLib::match(str, "POST"))      return POST;
    else if(StringLib::match(str, "PUT"))       return PUT;
    else if(StringLib::match(str, "DELETE"))    return DELETE;
    else if(StringLib::match(str, "TRACE"))     return TRACE;
    else if(StringLib::match(str, "OPTIONS"))   return OPTIONS;
    else if(StringLib::match(str, "CONNECT"))   return CONNECT;
    else                                        return UNRECOGNIZED;
}

/*----------------------------------------------------------------------------
 * verb2str
 *----------------------------------------------------------------------------*/
const char* EndpointObject::verb2str (verb_t verb)
{
    switch(verb)
    {
        case GET:       return "GET";
        case HEAD:      return "HEAD";
        case POST:      return "POST";
        case PUT:       return "PUT";
        case DELETE:    return "DELETE";
        case TRACE:     return "TRACE";
        case OPTIONS:   return "OPTIONS";
        case CONNECT:   return "CONNECT";
        default:        return "UNRECOGNIZED";
    }
}

/*----------------------------------------------------------------------------
 * str2code
 *----------------------------------------------------------------------------*/
EndpointObject::code_t EndpointObject::str2code (const char* str)
{
         if(StringLib::match(str, "OK"))                        return OK;
    else if(StringLib::match(str, "Bad Request"))               return Bad_Request;
    else if(StringLib::match(str, "Not Found"))                 return Not_Found;
    else if(StringLib::match(str, "Method Not Allowed"))        return Method_Not_Allowed;
    else if(StringLib::match(str, "Request Timeout"))           return Request_Timeout;
    else if(StringLib::match(str, "Method Not Implemented"))    return Method_Not_Implemented;
    else                                                        return Bad_Request;
}

/*----------------------------------------------------------------------------
 * code2str
 *----------------------------------------------------------------------------*/
const char* EndpointObject::code2str (code_t code)
{
    switch(code)
    {
        case OK:                        return "OK";
        case Bad_Request:               return "Bad Request";
        case Not_Found:                 return "Not Found";
        case Method_Not_Allowed:        return "Method Not Allowed";
        case Request_Timeout:           return "Request Timeout";
        case Method_Not_Implemented:    return "Method Not Implemented";
        default:                        break;
    }
    return "Bad Request";
}

/*----------------------------------------------------------------------------
 * buildheader
 *----------------------------------------------------------------------------*/
int EndpointObject::buildheader (char hdr_str[MAX_HDR_SIZE], code_t code, const char* content_type, int content_length, const char* transfer_encoding, const char* server)
{
    char str_buf[MAX_HDR_SIZE];

    StringLib::format(hdr_str, MAX_HDR_SIZE, "HTTP/1.1 %d %s\r\n", code, code2str(code));

    if(server)              StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Server: %s\r\n",               server),            MAX_HDR_SIZE);
    if(content_type)        StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Content-Type: %s\r\n",         content_type),      MAX_HDR_SIZE);
    if(content_length)      StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Content-Length: %d\r\n",       content_length),    MAX_HDR_SIZE);
    if(transfer_encoding)   StringLib::concat(hdr_str, StringLib::format(str_buf, MAX_HDR_SIZE, "Transfer-Encoding: %s\r\n",    transfer_encoding), MAX_HDR_SIZE);

    StringLib::concat(hdr_str, "\r\n",  MAX_HDR_SIZE);

    return StringLib::size(hdr_str, MAX_HDR_SIZE);
}
