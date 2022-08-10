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
    int rqst_len = 0;

    try
    {
        /* Calculate Content Length */
        int content_length = StringLib::size(data, MAX_RQST_BUF_LEN);
        if(content_length == MAX_RQST_BUF_LEN)
        {
            throw RunTimeException(ERROR, RTE_ERROR, "data exceeds maximum allowed size: %d > %d", content_length, MAX_RQST_BUF_LEN);
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
            if(rqst_len <= MAX_RQST_BUF_LEN)
            {
                LocalLib::copy(rqstBuf, rqst_hdr.getString(), hdr_len);
                LocalLib::copy(&rqstBuf[hdr_len], data, content_length);
            }
            else
            {
                throw RunTimeException(ERROR, RTE_ERROR, "request exceeds maximum length: %d", rqst_len);
            }
        }
        else if(content_length > 0)
        {
            /* Build Raw Request */
            rqst_len = content_length;
            if(rqst_len <= MAX_RQST_BUF_LEN)
            {
                LocalLib::copy(rqstBuf, data, content_length);
            }
            else
            {
                throw RunTimeException(ERROR, RTE_ERROR, "request exceeds maximum length: %d", rqst_len);
            }
        }
        else
        {
            /* Invalid Request */
            throw RunTimeException(ERROR, RTE_ERROR, "raw requests cannot be null");
        }

        /* Issue Request */
        int bytes_written = sock->writeBuffer(rqstBuf, rqst_len);

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

    /* Return Status */
    return status;
}
/*----------------------------------------------------------------------------
 * parseResponse
 *----------------------------------------------------------------------------*/
HttpClient::rsps_t HttpClient::parseResponse (Publisher* outq)
{
    rsps_t rsps = {
        .code = EndpointObject::OK,
        .response = NULL,
        .size = 0
    };

    int     header_num          = 0;
    int     rsps_index          = 0;
    int     rsps_buf_index      = 0;
    int     attempts            = 0;
    int     content_remaining   = 0;
    bool    headers_complete    = false;
    bool    response_complete   = false;

    /* Process Response */
    try
    {
        while(!response_complete)
        {
            int bytes_read = sock->readBuffer(&rspsBuf[rsps_buf_index], MAX_RSPS_BUF_LEN-rsps_buf_index);
            if(bytes_read > 0)
            {
                int line_start = 0;
                int line_term = 0;
                while(line_start < bytes_read)
                {
                    if(!headers_complete)
                    {
                        /* Parse Line */
                        line_term = parseLine(line_start, bytes_read);
                        if(line_term > 0) // valid header line found
                        {
                            /* Parse Status Line */
                            if(header_num == 0)
                            {
                                rsps.code = parseStatusLine(line_start, line_term);
                            }
                            /* Parse Header Line */
                            else
                            {
                                hdr_kv_t hdr = parseHeaderLine(line_start, line_term);
                                /* Process Content Length Header */
                                if(StringLib::match(hdr.key, "Content-Length"))
                                {
                                    long content_length;
                                    if(StringLib::str2long(hdr.value, &content_length))
                                    {
                                        content_remaining = content_length;
                                        rsps.size = content_length;
                                        if(!outq)
                                        {
                                            rsps.response = new char [rsps.size + 1]; // add one byte for terminator
                                            rsps.response[rsps.size] = '\0'; // ensure termination
                                        }

                                    }
                                    else
                                    {
                                        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid content length header => %s: %s", hdr.key, hdr.value);
                                    }
                                }
                            }

                            /* Go To Next Header */
                            line_start = line_term;
                            rsps_buf_index = 0;
                            header_num++;
                        }
                        else if(line_term < 0) // end of headers reached
                        {
                            line_term = line_start + 2; // move past header delimeter
                            headers_complete = true;
                        }
                        else // header line not complete (line_term == 0)
                        {
                            int bytes_remaining = bytes_read - line_start;
                            LocalLib::move(&rspsBuf[0], &rspsBuf[line_start], bytes_remaining);
                            rsps_buf_index += bytes_remaining;
                            break;
                        }
                    }
                    else // payload
                    {
                        /* Determine Bytes to Copy */
                        int bytes_remaining = bytes_read - line_term;
                        if(bytes_remaining > content_remaining)
                        {
                            throw RunTimeException(CRITICAL, RTE_ERROR, "Received too many bytes in response - %d > %d", bytes_remaining, content_remaining);
                        }
                        else if(bytes_remaining > 0)
                        {
                            if(outq)
                            {
                                /* Post Response */
                                int post_status = outq->postCopy(&rspsBuf[line_term], bytes_remaining, SYS_TIMEOUT);
                                if(post_status <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to post response: %d", post_status);
                            }
                            else
                            {
                                /* Populate Response */
                                LocalLib::copy(&rsps.response[rsps_index], &rspsBuf[line_term], bytes_remaining);
                                rsps_index += bytes_remaining;
                            }

                            /* Update Indices Check If Done */
                            content_remaining -= bytes_remaining;
                            if(content_remaining <= 0) response_complete = true;
                        }

                        /* Update Indices */
                        line_term = 0;
                        break;
                    }
                }
            }
            else if(bytes_read == TIMEOUT_RC)
            {
                if(++attempts >= MAX_TIMEOUTS)
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Maximum number of attempts reached");
                }
            }
            else
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "Failed to read socket: %d", bytes_read);
            }
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to process response: %s", e.what());
        rsps.code = EndpointObject::Internal_Server_Error;
    }

    /* Return Response */
    return rsps;
}

/*----------------------------------------------------------------------------
 * parseLine
 *----------------------------------------------------------------------------*/
long HttpClient::parseLine (int start, int end)
{
    if( ((end - start) >= 2) &&
        rspsBuf[start+0] == '\r' && rspsBuf[start+1] == '\n')
    {
        return -1; // return end of headers;
    }

    for(int i = start; i < (end - 1); i++)
    {
        if(rspsBuf[i] == '\r' && rspsBuf[i+1] == '\n')
        {
            rspsBuf[i] = '\0'; // terminate header
            return i+2; // return header line term
        }
    }

    return 0; // return absence of header line term
}

/*----------------------------------------------------------------------------
 * parseStatusLine
 *----------------------------------------------------------------------------*/
EndpointObject::code_t HttpClient::parseStatusLine (int start, int term)
{
    long code_val = EndpointObject::Internal_Server_Error;

    /* Find Start of Response Code */
    int code_str_start = term;
    for(int i = start; i < term; i++)
    {
        if(rspsBuf[i] == ' ')
        {
            code_str_start = i+1;
            break;
        }
    }

    /* Find End of Response Code */
    int code_str_term = term;
    for(int j = code_str_start;  j < term; j++)
    {
        if(rspsBuf[j] == ' ')
        {
            code_str_term = j+1;
            break;
        }
    }

    /* Determine Response Code */
    if(code_str_term < term)
    {
        const char* code_str = &rspsBuf[code_str_start];
        rspsBuf[code_str_term] = '\0';
        if(!StringLib::str2long(code_str, &code_val))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid code: %s", code_str);
        }
    }

    /* Return Code */
    return (EndpointObject::code_t)code_val;
}

/*----------------------------------------------------------------------------
 * parseHeaderLine
 *----------------------------------------------------------------------------*/
HttpClient::hdr_kv_t HttpClient::parseHeaderLine (int start, int term)
{
    hdr_kv_t hdr = {
        .key = &rspsBuf[start],
        .value = NULL
    };

    for(int i = start; i < (term - 2); i++)
    {
        if(rspsBuf[i] == ':' && rspsBuf[i+1] == ' ')
        {
            rspsBuf[i] = '\0'; // terminate key string
            hdr.value = &rspsBuf[i+2];
            break;
        }
    }

    return hdr;
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