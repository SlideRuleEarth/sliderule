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
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
HttpClient::~HttpClient(void)
{
    active = false;
    connMutex.lock();
    {
        for(int i = 0; i < connections.length(); i++)
        {
            connection_t* connection = connections[i];
            connection->active = false;
            delete connection->pid;
            delete connection;
        }
    }
    connMutex.unlock();

    if(ipAddr) delete [] ipAddr;
}

/*----------------------------------------------------------------------------
 * request
 *----------------------------------------------------------------------------*/
const char* HttpClient::request (EndpointObject::verb_t verb, const char* resource, const char* data, const char* outq_name)
{
    connection_t* connection = NULL;
    Subscriber* inq = NULL;
    char* response = NULL;

    /* Allocate and Initialize Connection */
    connection = new connection_t;
    connection->active = true;
    connection->keep_alive = true;
    connection->verb = verb;
    connection->resource = StringLib::duplicate(resource);
    connection->data = StringLib::duplicate(data);
    connection->outq = new Publisher(outq_name);
    connection->client = this;
    connection->status = CONNECTION_ERROR;

    /* Check if Blocking */
    if(outq_name == NULL)
    {
        /* Create Subscriber */
        inq = new Subscriber(*(connection->outq));
    }

    /* Start Connection Thread */
    connMutex.lock();
    {
        connection->pid = new Thread(requestThread, connection);
        connections.add(connection);
    }
    connMutex.unlock();

    /* Check if Blocking */
    if(outq_name == NULL)
    {
        /* Setup Response Variables */
        int total_response_length = 0;
        List<Subscriber::msgRef_t> responses;

        /* Read Responses */
        int attempts = 0;
        bool done = false;
        while(!done)
        {
            Subscriber::msgRef_t ref;
            int recv_status = inq->receiveRef(ref, SYS_TIMEOUT);
            if(recv_status > 0)
            {
                if(ref.size > 0)
                {
                    total_response_length += ref.size;
                    responses.add(ref);
                }
                else // terminator
                {
                    inq->dereference(ref);
                    done = true;
                }
            }
            else if(recv_status == TIMEOUT_RC)
            {
                attempts++;
                if(attempts >= MAX_TIMEOUTS)
                {
                    mlog(CRITICAL, "Maximum number of attempts to read queue reached");
                    done = true;
                }
            }
            else
            {
                mlog(CRITICAL, "Failed queue receive on %s with error %d", getName(), recv_status);
                done = true;
            }
        }

        /* Allocate and Build Response Array */
        response = new char [total_response_length + 1];
        List<Subscriber::msgRef_t>::Iterator iterator(responses);
        int index = 0;
        for(int i = 0; i < responses.length(); i++)
        {
            LocalLib::copy(&response[index], iterator[i].data, iterator[i].size);
            index += iterator[i].size;
            inq->dereference((Subscriber::msgRef_t&)iterator[i]);
        }
        response[index] = '\0';
    }

    /* Delete Subscriber */
    if(inq) delete inq;

    /* return response */
    return response;
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
 * requestThread
 *----------------------------------------------------------------------------*/
void* HttpClient::requestThread(void* parm)
{
    connection_t* connection = (connection_t*)parm;

    /* Initialize Connection Status */
    connection->status = RSPS_INCOMPLETE;

    /* Calculate Content Length */
    int content_length = StringLib::size(connection->data, MAX_RQST_DATA_LEN);
    if(content_length == MAX_RQST_DATA_LEN)
    {
        mlog(ERROR, "Http request data exceeds maximum allowed size: %d > %d", content_length, MAX_RQST_DATA_LEN);
    }
    else
    {
        /* Set Keep Alive Header */
        const char* keep_alive_header = "";
        if(connection->keep_alive) keep_alive_header = "Connection: keep-alive\r\n";

        /* Build Request*/
        unsigned char* rqst = NULL;
        int rqst_len = 0;
        if(connection->verb != EndpointObject::RAW)
        {
            /* Build Request Header */
            SafeString rqst_hdr("%s %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: sliderule/%s\r\nAccept: */*\r\n%sContent-Length: %d\r\n\r\n",
                                EndpointObject::verb2str(connection->verb),
                                connection->resource,
                                connection->client->ipAddr,
                                LIBID,
                                keep_alive_header,
                                content_length);

            /* Build Request */
            int hdr_len = rqst_hdr.getLength() - 1; // minus one to remove null termination of rqst_hdr
            rqst_len = content_length + hdr_len;
            rqst = new unsigned char [rqst_len];
            LocalLib::copy(rqst, rqst_hdr.getString(), hdr_len);
            LocalLib::copy(&rqst[hdr_len], connection->data, content_length);
        }
        else if(content_length > 0)
        {
            /* Build Raw Request */
            rqst = new unsigned char [content_length];
            rqst_len = content_length;
            LocalLib::copy(rqst, connection->data, content_length);
        }
        else
        {
            /* Invalid Request */
            mlog(ERROR, "Invalid HTTP request - raw requests cannot be null");
            connection->status = RQST_INVALID;
            return NULL; // no memory allocated at this point
        }

        /* Establish Connection */
        int connection_attempts = MAX_TIMEOUTS;
        bool die_on_disconnect = false; // only used for non-blocking/threaded connections
        TcpSocket sock(NULL, connection->client->getIpAddr(), connection->client->getPort(), false, NULL, die_on_disconnect);
        while(!sock.isConnected() && connection_attempts--) LocalLib::sleep(1);

        /* Issue Request */
        int bytes_written = sock.writeBuffer(rqst, rqst_len);
        if(bytes_written != rqst_len)
        {
            mlog(CRITICAL, "Http request failed to send request: act=%d, exp=%d", bytes_written, rqst_len);
            connection->status = RQST_FAILED_TO_SEND;
        }
        else
        {
            /* Allocate Response Buffer */
            char* rsps = new char [RSPS_READ_BUF_LEN];

            /* Initialize Parsing Variables */
            SafeString rsps_header;
//            char response_code_buf[MAX_DIGITS];
//            char content_length_buf[MAX_DIGITS];
//            const char* content_length_label = "Content-Length: ";
            bool header_complete = false;

            /* Read Response */
            int attempts = 0;
            bool done = false;
            while(!done && connection->client->active)
            {
                unsigned char* rsps_data = NULL;
                int rsps_size = 0;

                int bytes_read = sock.readBuffer(rsps, RSPS_READ_BUF_LEN - 1);
                if(bytes_read > 0)
                {
                    if(!header_complete)
                    {
                        int index = 0;

                        /* Check for Partial Delimeter */
                        bool possible_partial_delim = false;
                        while(rsps[index] == '\r' || rsps[index] == '\n')
                        {
                            possible_partial_delim = true;
                            rsps_header.appendChar(rsps[index]);
                            index++;
                        }

                        /* Check for Header Complete when Partial Delimeter Read */
                        int possible_header_len = rsps_header.getLength();
                        if(possible_partial_delim && possible_header_len > 4)
                        {
                            if( (rsps_header[possible_header_len - 5] == '\r') &&
                                (rsps_header[possible_header_len - 4] == '\n') &&
                                (rsps_header[possible_header_len - 3] == '\r') &&
                                (rsps_header[possible_header_len - 2] == '\n') )
                            {
                                header_complete = true;
                                rsps_data = (unsigned char*)&rsps[index];
                                rsps_size = bytes_read - index;
                            }
                        }

                        /* If Header still not Complete */
                        if(!header_complete)
                        {
                            /* Look for Delimeter */
                            while(index < (bytes_read - 4))
                            {
                                if((rsps[index + 0] == '\r') &&
                                   (rsps[index + 1] == '\n') &&
                                   (rsps[index + 2] == '\r') &&
                                   (rsps[index + 3] == '\n') )
                                {
                                    break;
                                }
                                else
                                {
                                    index++;
                                }
                            }

                            /* Check if Header Complete */
                            if(rsps[index] == '\r')
                            {
                                connection->status = CONNECTION_OKAY;
                                header_complete = true;
                                rsps[index] = '\0';
                                index += 4; // move past delimeter
                                rsps_data = (unsigned char*)&rsps[index];
                                rsps_size = bytes_read - index;
                            }

                            /* Append Bytes Read to Response Header */
                            rsps_header += rsps;
                        }
                    }
                    else
                    {
                        /* Pass Through Everything Read */
                        rsps_data = (unsigned char*)rsps;
                        rsps_size = bytes_read;
                    }

                    /* Post Contents */
                    if(rsps_size > 0)
                    {
                        int post_status = connection->outq->postCopy(rsps_data, rsps_size);
                        if(post_status <= 0)
                        {
                            mlog(CRITICAL, "Failed to post response: %d", post_status);
                            connection->status = RSPS_FAILED_TO_POST;
                            done = true;
                        }
                    }
                }
                else if(bytes_read == SHUTDOWN_RC)
                {
                    done = true;
                }
                else if(bytes_read == TIMEOUT_RC)
                {
                    attempts++;
                    if(attempts >= MAX_TIMEOUTS)
                    {
                        mlog(CRITICAL, "Maximum number of attempts to read response reached");
                        done = true;
                    }
                }
                else
                {
                    mlog(CRITICAL, "Failed to read socket for response: %d", bytes_read);
                    done = true;
                }
            }

            /* Clean Up Response Buffer */
            delete [] rsps;
        }

        /* Clean Up Request Buffer */
        delete [] rqst;
    }

    /* Post Terminator */
    int term_status = connection->outq->postCopy("", 0);
    if(term_status < 0)
    {
        mlog(CRITICAL, "Failed to post terminator in http client: %d", term_status);
        connection->status = RSPS_FAILED_TO_POST;
    }

    /* Clean Up Connection */
    delete connection->outq;
    delete [] connection->data;
    delete [] connection->resource;

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaRequest - :request(<verb>, <resource>, <data>, [<outq>])
 *----------------------------------------------------------------------------*/
int HttpClient::luaRequest (lua_State* L)
{
    bool status = false;
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
        EndpointObject::verb_t verb = EndpointObject::str2verb(verb_str);
        if(verb == EndpointObject::UNRECOGNIZED)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid verb: %s", verb_str);
        }

        /* Make Request */
        const char* response = lua_obj->request(verb, resource, data, outq_name);
        if(response)
        {
            /* Return Respnonse String */
            int total_response_length = StringLib::size(response, RSPS_LUA_BUF_LEN);
            lua_pushlstring(L, response, total_response_length + 1);
            delete [] response;
            num_rets++;
        }
        else
        {
            status = true;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error initiating request: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_rets);
}
