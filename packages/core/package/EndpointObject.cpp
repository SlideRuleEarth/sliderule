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
const char* EndpointObject::LUA_RESPONSE_QUEUE = "rspq";
const char* EndpointObject::LUA_REQUEST_ID = "rqstid";

FString EndpointObject::serverHead("sliderule/%s", LIBID);

/******************************************************************************
 * AUTHENTICATOR SUBCLASS
 ******************************************************************************/

 const char* EndpointObject::Authenticator::OBJECT_TYPE = "Authenticator";
 const char* EndpointObject::Authenticator::LUA_META_NAME = "Authenticator";
 const struct luaL_Reg EndpointObject::Authenticator::LUA_META_TABLE[] = {
     {NULL,          NULL}
 };

 /*----------------------------------------------------------------------------
  * Constructor
  *----------------------------------------------------------------------------*/
 EndpointObject::Authenticator::Authenticator(lua_State* L):
     LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
 {
 }

 /*----------------------------------------------------------------------------
  * Destructor
  *----------------------------------------------------------------------------*/
 EndpointObject::Authenticator::~Authenticator(void) = default;

 /******************************************************************************
 * REQUEST SUBCLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointObject::Request::Request (const char* _id):
    path        (NULL),
    resource    (NULL),
    verb        (UNRECOGNIZED),
    version     (NULL),
    headers     (EXPECTED_MAX_HEADER_FIELDS),
    body        (NULL),
    length      (0),
    trace_id    (ORIGIN),
    id          (StringLib::duplicate(_id))
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EndpointObject::Request::~Request (void)
{
    delete [] body;
    delete [] resource;
    delete [] version;
    delete [] path;
    delete [] id;
}

/*----------------------------------------------------------------------------
 * getHdrSourceIp
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrSourceIp (void) const
{
    string* hdr_str;
    if(headers.find("x-forwarded-for", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "0.0.0.0";
}

/*----------------------------------------------------------------------------
 * getHdrClient
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrClient (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-client", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "unknown";
}

/*----------------------------------------------------------------------------
 * getHdrAccount
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrAccount (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-account", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return "anonymous";
}

/*----------------------------------------------------------------------------
 * getHdrStreaming
 *----------------------------------------------------------------------------*/
const char* EndpointObject::Request::getHdrStreaming (void) const
{
    string* hdr_str;
    if(headers.find("x-sliderule-streaming", &hdr_str))
    {
        return hdr_str->c_str();
    }
    return NULL; // special default case
}

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointObject::EndpointObject (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table),
    authenticator(NULL)
{
    LuaEngine::setAttrFunc(L, "auth", luaAuth);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EndpointObject::~EndpointObject (void)
{
    if(authenticator) authenticator->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * authorize
 *----------------------------------------------------------------------------*/
bool EndpointObject::authenticate (Request* request) const
{
    bool authorized = true;
    if(authenticator)
    {
        char* bearer_token = NULL;

        /* Extract Bearer Token */
        string* auth_hdr;
        if(request->headers.find("authorization", &auth_hdr))
        {
            bearer_token = StringLib::find(auth_hdr->c_str(), ' ');
            if(bearer_token) bearer_token += 1;
        }

        /* Validate Bearer Token */
        authorized = authenticator->isValid(bearer_token);
    }
    return authorized;
}

/*----------------------------------------------------------------------------
 * str2verb
 *----------------------------------------------------------------------------*/
EndpointObject::verb_t EndpointObject::str2verb (const char* str)
{
    if(StringLib::match(str, "GET"))       return GET;
    if(StringLib::match(str, "HEAD"))      return HEAD;
    if(StringLib::match(str, "POST"))      return POST;
    if(StringLib::match(str, "PUT"))       return PUT;
    if(StringLib::match(str, "DELETE"))    return DELETE;
    if(StringLib::match(str, "TRACE"))     return TRACE;
    if(StringLib::match(str, "OPTIONS"))   return OPTIONS;
    if(StringLib::match(str, "CONNECT"))   return CONNECT;
    if(StringLib::match(str, "RAW"))       return RAW;
    return UNRECOGNIZED;
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
    if(StringLib::match(str, "Bad Request"))               return Bad_Request;
    if(StringLib::match(str, "Not Found"))                 return Not_Found;
    if(StringLib::match(str, "Method Not Allowed"))        return Method_Not_Allowed;
    if(StringLib::match(str, "Request Timeout"))           return Request_Timeout;
    if(StringLib::match(str, "Method Not Implemented"))    return Method_Not_Implemented;
    return Bad_Request;
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

    return StringLib::size(hdr_str);
}

/*----------------------------------------------------------------------------
 * luaAuth - :auth(<authentication object>)
 *
 * Note: NOT thread safe, must be called prior to attaching endpoint to server
 *----------------------------------------------------------------------------*/
int EndpointObject::luaAuth (lua_State* L)
{
    bool status = false;
    Authenticator* auth = NULL;

    try
    {
        /* Get Self */
        EndpointObject* lua_obj = dynamic_cast<EndpointObject*>(getLuaSelf(L, 1));

        /* Get Authenticator */
        auth = dynamic_cast<Authenticator*>(getLuaObject(L, 2, EndpointObject::Authenticator::OBJECT_TYPE));

        /* Set Authenticator */
        lua_obj->authenticator = auth;

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        if(auth) auth->releaseLuaObject();
        mlog(e.level(), "Error setting authenticator: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
