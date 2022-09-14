/*
 * Copyright (c) 2021, University of Washington
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the University of Washington nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "EndpointObject.h"
#include "EventLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EndpointObject::OBJECT_TYPE = "EndpointObject";

/******************************************************************************
 * REQUEST SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointObject::Request::Request (const char* _id):
    headers(EXPECTED_MAX_HEADER_FIELDS)
{
    id          = StringLib::duplicate(_id);
    path        = NULL;
    resource    = NULL;
    verb        = UNRECOGNIZED;
    body        = NULL;
    length      = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EndpointObject::Request::~Request (void)
{
    /* Clear Out Headers */
    const char* header;
    const char* key = headers.first(&header);
    while(key != NULL)
    {
        delete [] header;
        key = headers.next(&header);
    }

    /* Free Allocate Members */
    if(body) delete [] body;
    if(resource) delete [] resource;
    if(path) delete [] path;
}

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
    else if(StringLib::match(str, "RAW"))       return RAW;
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
        case RAW:       return "";
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
