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

#include <atomic>

#include "HttpClient.h"
#include "EndpointObject.h"
#include "LuaEngine.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* HttpClient::OBJECT_TYPE = "HttpClient";
const char* HttpClient::LuaMetaName = "HttpClient";
const struct luaL_Reg HttpClient::LuaMetaTable[] = {
    {"request",     luaRequest},
    {"connected",   luaConnected},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - server(<ip_addr>, <port>)
 *----------------------------------------------------------------------------*/
int HttpClient::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* ip_addr = getLuaString(L, 1, true, NULL);
        int         port    = (int)getLuaInteger(L, 2);

        /* Get Server Parameter */
        if( ip_addr && (StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*")) )
        {
            ip_addr = NULL;
        }

        /* Return Http Client Object */
        return createLuaObject(L, new HttpClient(L, ip_addr, port));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating HttpClient: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
HttpClient::HttpClient(lua_State* L, const char* _ip_addr, int _port):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    active = true;
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;
    sock = initializeSocket(ipAddr, port);
    requestPub = new Publisher(NULL);
    requestPid = NULL;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
HttpClient::HttpClient(lua_State* L, const char* url):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    // Initial Settings
    active = false;
    ipAddr = NULL;
    port = -1;

    // Parse URL
    char url_buf[MAX_URL_LEN];
    StringLib::copy(url_buf, url, MAX_URL_LEN);
    char* proto_term = StringLib::find(url_buf, "://", MAX_URL_LEN);
    if(proto_term)
    {
        char* proto = url_buf;
        char* _ip_addr = proto_term + 3;
        *proto_term = '\0';
        if((_ip_addr - proto) < MAX_URL_LEN)
        {
            char* ip_addr_term = StringLib::find(_ip_addr, ":", MAX_URL_LEN);
            if(ip_addr_term)
            {
                char* _port_str = ip_addr_term + 1;
                *ip_addr_term = '\0';
                if(_port_str)
                {
                    long val;
                    if(StringLib::str2long(_port_str, &val))
                    {
                        active = true;
                        ipAddr = StringLib::duplicate(_ip_addr);
                        port = val;
                    }
                }
            }
        }
    }

    // Create Socket Connection
    sock = initializeSocket(ipAddr, port);

    // Create Request Queue and Thread
    requestPub = new Publisher(NULL);
    requestPid = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
HttpClient::~HttpClient(void)
{
    active = false;
    if(requestPid) delete requestPid;
    if(requestPub) delete requestPub;
    if(ipAddr) delete [] ipAddr;
    if(sock) delete sock;
}

/*----------------------------------------------------------------------------
 * request
 *----------------------------------------------------------------------------*/
HttpClient::rsps_t HttpClient::request (EndpointObject::verb_t verb, const char* resource, const char* data, bool keep_alive, Publisher* outq)
{
    if(makeRequest(verb, resource, data, keep_alive))
    {
        rsps_t rsps = parseResponse(outq);
        return rsps;
    }
    else
    {
        rsps_t rsps = {
            .code = EndpointObject::Service_Unavailable,
            .response = NULL,
            .size = 0
        };
        return rsps;
    }
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* HttpClient::getIpAddr (void)
{
    if(ipAddr)  return ipAddr;
    else        return "0.0.0.0";
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int HttpClient::getPort (void)
{
    return port;
}

/*----------------------------------------------------------------------------
 * initializeSocket
 *----------------------------------------------------------------------------*/
TcpSocket* HttpClient::initializeSocket(const char* _ip_addr, int _port)
{
    bool block = false;
    return new TcpSocket(NULL, _ip_addr, _port, false, &block, false);
}

/*----------------------------------------------------------------------------
 * makeRequest
 *----------------------------------------------------------------------------*/
bool HttpClient::makeRequest (EndpointObject::verb_t verb, const char* resource, const char* data, bool keep_alive)
{
    bool status = true;
    unsigned char* rqst = NULL;
    int rqst_len = 0;

    try
    {
        /* Calculate Content Length */
        int content_length = StringLib::size(data, MAX_RQST_DATA_LEN);
        if(content_length == MAX_RQST_DATA_LEN)
        {
            throw RunTimeException(ERROR, RTE_ERROR, "data exceeds maximum allowed size: %d > %d", content_length, MAX_RQST_DATA_LEN);
        }

        /* Set Keep Alive Header */
        const char* keep_alive_header = "";
        if(keep_alive) keep_alive_header = "Connection: keep-alive\r\n";

        /* Build Request */
        if(verb != EndpointObject::RAW)
        {
            /* Build Request Header */
            SafeString rqst_hdr("%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: sliderule/%s\r\nAccept: */*\r\n%sContent-Length: %d\r\n\r\n",
                                EndpointObject::verb2str(verb),
                                resource,
                                getIpAddr(),
                                LIBID,
                                keep_alive_header,
                                content_length);

            /* Build Request */
            int hdr_len = rqst_hdr.getLength() - 1; // minus one to remove null termination of rqst_hdr
            rqst_len = content_length + hdr_len;
            rqst = new unsigned char [rqst_len];
            LocalLib::copy(rqst, rqst_hdr.getString(), hdr_len);
            LocalLib::copy(&rqst[hdr_len], data, content_length);
        }
        else if(content_length > 0)
        {
            /* Build Raw Request */
            rqst = new unsigned char [content_length];
            rqst_len = content_length;
            LocalLib::copy(rqst, data, content_length);
        }
        else
        {
            /* Invalid Request */
            throw RunTimeException(ERROR, RTE_ERROR, "raw requests cannot be null");
        }

        /* Issue Request */
        int bytes_written = sock->writeBuffer(rqst, rqst_len);

        /* Check Status */
        if(bytes_written != rqst_len)
        {
            throw RunTimeException(ERROR, RTE_ERROR, "failed to send request: act=%d, exp=%d", bytes_written, rqst_len);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "HTTP Request Failed: %s", e.what());
        status = false;
    }

    /* Clean Up */
    if(rqst) delete [] rqst;

    /* Return Status */
    return status;
}
/*----------------------------------------------------------------------------
 * parseResponse
 *----------------------------------------------------------------------------*/
HttpClient::rsps_t HttpClient::parseResponse (Publisher* outq)
{
    /* Response Variables */
    long rsps_index = 0;
    rsps_t rsps = {
        .code = EndpointObject::OK,
        .response = NULL,
        .size = 0
    };

    /* Define Response Processing Variables */
    StringLib::TokenList* headers = NULL;
    StringLib::TokenList* response_line = NULL;
    char* rsps_buf = NULL;
    const char* code_msg = NULL;
    long content_length = 0;

    /* Process Response */
    try
    {
        /* Attempt to Fill Resopnse Buffer */
        rsps_buf = new char [MAX_RSPS_BUF_LEN + 1];
        long rsps_buf_index = 0;
        int attempts = 0;
        while(true)
        {
            int bytes_to_read = MAX_RSPS_BUF_LEN - rsps_buf_index;
            if(bytes_to_read > 0)
            {
                int bytes_read = sock->readBuffer(&rsps_buf[rsps_buf_index], bytes_to_read);
                if(bytes_read > 0)
                {
                    rsps_buf_index += bytes_read;
                }
                else if(bytes_read == TIMEOUT_RC)
                {
                    if(++attempts >= MAX_TIMEOUTS)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "Maximum number of attempts reached on first read");
                    }
                }
                else
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Failed first read of socket: %d", bytes_read);
                }
            }
            else
            {
                break;
            }
        }

        /* Look for Header Delimeter */
        bool hdr_found = false;
        long hdr_index = 0;
        while(hdr_index < (rsps_buf_index - 4))
        {
            if( (rsps_buf[hdr_index + 0] == '\r') &&
                (rsps_buf[hdr_index + 1] == '\n') &&
                (rsps_buf[hdr_index + 2] == '\r') &&
                (rsps_buf[hdr_index + 3] == '\n') )
            {
                hdr_found = true;               // complete header is present
                rsps_buf[hdr_index + 0] = '\0'; // null terminate header
                hdr_index += 4;                 // move hdr index to first byte of payload
                break;                          // exit loop
            }
            else
            {
                hdr_index++;
            }
        }

        /* Error Check Header Not Complete */
        if(!hdr_found) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to read complete header");

        /* Parse Headers */
        headers = StringLib::split(rsps_buf, hdr_index, '\r', true);
        response_line = StringLib::split(headers->get(0), MAX_RSPS_BUF_LEN, ' ', true);

        /* Get Response Code */
        long val;
        if(StringLib::str2long(response_line->get(1), &val))
        {
            rsps.code = (EndpointObject::code_t)val;
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid code: %s", response_line->get(1));
        }

        /* Get Response Message */
        code_msg = StringLib::duplicate(response_line->get(2));

        /* Process Each Header */
        for(int h = 1; h < headers->length(); h++)
        {
            StringLib::TokenList* header = NULL;
            try
            {
                header = StringLib::split(headers->get(h), MAX_RSPS_BUF_LEN, ':', true);
                const char* key = header->get(0);
                const char* value = header->get(1);
                if(StringLib::match(key, "Content-Length"))
                {
                    if(!StringLib::str2long(value, &content_length))
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid content length header => %s: %s", key, value);
                    }
                }
            }
            catch(const RunTimeException& e)
            {
                mlog(CRITICAL, "<%s>: Failed to parse header", e.what());
            }
            if(header) delete header;
        }

        /* Check Content Length */
        if(content_length <= 0)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid content length: %ld", content_length);
        }

        /* Set Size of Response */
        rsps.size = content_length;

        /* Determine Number of Payload Bytes Already Read */
        int payload_bytes_to_copy = rsps_buf_index - hdr_index;
        payload_bytes_to_copy = MIN(payload_bytes_to_copy, content_length);

        if(outq)
        {
            /* Post Response Already Read */
            int post_status1 = outq->postCopy(&rsps_buf[hdr_index], payload_bytes_to_copy, SYS_TIMEOUT);
            if(post_status1 <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed initial post: %d", post_status1);
            rsps_index += payload_bytes_to_copy;

            /* Read and Populate Rest of Response */
            while(rsps_index < content_length)
            {
                int payload_bytes_to_read = content_length - rsps_index;
                int bytes_read = sock->readBuffer(rsps_buf, payload_bytes_to_read);
                if(bytes_read > 0)
                {
                    int post_status2 = outq->postCopy(rsps_buf, bytes_read, SYS_TIMEOUT);
                    if(post_status2 <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed continued post: %d", post_status2);
                    rsps_index += bytes_read;
                }
                else if(bytes_read == TIMEOUT_RC)
                {
                    if(++attempts >= MAX_TIMEOUTS)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "Maximum number of attempts reached continuing to stream response");
                    }
                }
                else
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to continue to read socket for stream: %d", bytes_read);
                }
            }
        }
        else
        {
            /* Allocate Response */
            rsps.response = new char [rsps.size];

            /* Populate Response Already Read */
            if(payload_bytes_to_copy > 0)
            {
                LocalLib::copy(rsps.response, &rsps_buf[hdr_index], payload_bytes_to_copy);
                rsps_index += payload_bytes_to_copy;
            }

            /* Read and Populate Rest of Response */
            while(rsps_index < rsps.size)
            {
                int payload_bytes_to_read = rsps.size - rsps_index;
                int bytes_read = sock->readBuffer(&rsps.response[rsps_index], payload_bytes_to_read);
                if(bytes_read > 0)
                {
                    rsps_index += bytes_read;
                }
                else if(bytes_read == TIMEOUT_RC)
                {
                    if(++attempts >= MAX_TIMEOUTS)
                    {
                        throw RunTimeException(CRITICAL, RTE_ERROR, "Maximum number of attempts reached continuing to read response");
                    }
                }
                else
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to continue to read socket: %d", bytes_read);
                }
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to process response: %s", e.what());
        if(rsps.response) delete [] rsps.response;
        rsps.size = 0;
        rsps.code = EndpointObject::Internal_Server_Error;
    }

    /* Clean Up Response Processing Variables */
    if(code_msg) delete [] code_msg;
    if(response_line) delete response_line;
    if(headers) delete headers;
    if(rsps_buf) delete [] rsps_buf;

    /* Return Response */
    return rsps;
}

/*----------------------------------------------------------------------------
 * requestThread
 *----------------------------------------------------------------------------*/
void* HttpClient::requestThread(void* parm)
{
    HttpClient* client = (HttpClient*)parm;
    Subscriber* request_sub = new Subscriber(*(client->requestPub));

    while(client->active)
    {
        rqst_t rqst;
        int recv_status = request_sub->receiveCopy(&rqst, sizeof(rqst_t), SYS_TIMEOUT);
        if(recv_status > 0)
        {
            try
            {
                client->request(rqst.verb, rqst.resource, rqst.data, true, rqst.outq);
                delete [] rqst.resource;
                delete [] rqst.data;
                delete rqst.outq;
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Failure processing request: %s", e.what());
            }
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Failed to receive request: %d", recv_status);
            break;
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaRequest - :request(<verb>, <resource>, <data>, [<outq>])
 *----------------------------------------------------------------------------*/
int HttpClient::luaRequest (lua_State* L)
{
    bool status = true;
    int num_rets = 1;

    try
    {
        /* Get Self */
        HttpClient* lua_obj = (HttpClient*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* verb_str    = getLuaString(L, 2);
        const char* resource    = getLuaString(L, 3);
        const char* data        = getLuaString(L, 4);
        const char* outq_name   = getLuaString(L, 5, true, NULL);

        /* Error Check Verb */
        EndpointObject::verb_t verb =  EndpointObject::str2verb(verb_str);
        if(verb == EndpointObject::UNRECOGNIZED)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid verb: %s", verb_str);
        }

        /* Check if Blocking */
        if(outq_name == NULL)
        {
            rsps_t rsps = lua_obj->request(verb, resource, data, true, NULL);
            if(rsps.response)
            {
                lua_pushlstring(L, rsps.response, rsps.size);
                delete [] rsps.response;
                lua_pushinteger(L, rsps.code);
                num_rets += 2;
            }
            else
            {
                status = false;
                lua_pushnil(L);
                lua_pushinteger(L, rsps.code);
                num_rets += 2;
            }
            num_rets++;
        }
        else
        {
            /* Initialize Request */
            rqst_t rqst = {
                .verb = verb,
                .resource = StringLib::duplicate(resource),
                .data = StringLib::duplicate(data),
                .outq = new Publisher(outq_name)
            };

            /* Create Request Thread Upon First Request */
            if(!lua_obj->requestPid) lua_obj->requestPid = new Thread(requestThread, lua_obj);

            /* Post Request */
            status = lua_obj->requestPub->postCopy(&rqst, sizeof(rqst_t)) > 0;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error initiating request: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_rets);
}

/*----------------------------------------------------------------------------
 * luaConnected - :connected()
 *----------------------------------------------------------------------------*/
int HttpClient::luaConnected (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        HttpClient* lua_obj = (HttpClient*)getLuaSelf(L, 1);

        /* Determine Connection Status */
        if(lua_obj->sock)
        {
            status = lua_obj->sock->isConnected();
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error determining connection status: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}