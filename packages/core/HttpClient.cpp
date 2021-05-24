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
        const char* ip_addr         = getLuaString(L, 1, true, NULL);
        int         port            = (int)getLuaInteger(L, 2);

        /* Get Server Parameter */
        if( ip_addr && (StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*")) )
        {
            ip_addr = NULL;
        }

        /* Return File Device Object */
        return createLuaObject(L, new HttpClient(L, ip_addr, port));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating HttpClient: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
HttpClient::HttpClient(lua_State* L, const char* _ip_addr, int _port):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
HttpClient::~HttpClient(void)
{
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

    /* Calculate Content Length */
    int content_length = StringLib::size(connection->data, MAX_RQST_DATA_LEN);
    if(content_length == MAX_RQST_DATA_LEN)
    {
        mlog(ERROR, "Http request data exceeds maximum allowed size: %d > %d\n", content_length, MAX_RQST_DATA_LEN);
    }
    else
    {
        /* Set Keep Alive Header */
        const char* keep_alive_header = "";
        if(connection->keep_alive) keep_alive_header = "Connection: keep-alive\r\n";

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
        int rqst_len = content_length + hdr_len;
        unsigned char* rqst = new unsigned char [rqst_len];
        LocalLib::copy(rqst, rqst_hdr.getString(), hdr_len);
        LocalLib::copy(&rqst[hdr_len], connection->data, content_length);

        /* Establish Connection */
        bool block = false; // only try to connect once
        bool die_on_disconnect = false; // only used for non-blocking/threaded connections
        TcpSocket sock(NULL, connection->client->getIpAddr(), connection->client->getPort(), false, &block, die_on_disconnect);

        /* Issue Request */
        int bytes_written = sock.writeBuffer(rqst, rqst_len);
        if(bytes_written != rqst_len)
        {
            mlog(CRITICAL, "Http request failed to send request: act=%d, exp=%d\n", bytes_written, rqst_len);
        }
        else
        {
            /* Allocate Response Buffer */
            unsigned char* rsps = new unsigned char [RSPS_READ_BUF_LEN];

            /* Read Response */
            int attempts = 0;
            bool done = false;
            while(!done)
            {
                int bytes_read = sock.readBuffer(rsps, RSPS_READ_BUF_LEN);
                if(bytes_read > 0)
                {
                    int post_status = connection->outq->postCopy(rsps, bytes_read);
                    if(post_status <= 0)
                    {
                        mlog(CRITICAL, "Failed to post response: %d\n", post_status);
                        done = true;
                    }
                }
                else if(bytes_read == TIMEOUT_RC)
                {
                    attempts++;
                    if(attempts >= MAX_TIMEOUTS)
                    {
                        mlog(CRITICAL, "Maximum number of attempts to read response reached\n");
                        done = true;
                    }
                }
                else
                {
                    mlog(CRITICAL, "Failed to read socket for response: %d\n", bytes_read);
                    done = true;
                }
            }

            /* Clean Up Response Buffer */
            delete [] rsps;
        }

        /* Clean Up Request Buffer */
        delete [] rqst;
    }

    /* Clean Up Connection */
    delete connection->outq;    
    delete connection->data;
    delete connection->resource;

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaRequest - :request(<verb>, <resource>, <data>, [<outq>])
 *----------------------------------------------------------------------------*/
int HttpClient::luaRequest (lua_State* L)
{
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
            throw RunTimeException("Invalid verb: %s", verb_str);
        }

        /* Allocate Connection Structure */
        connection_t* connection = new connection_t;

        /* Initialize Connection */
        connection->active = true;
        connection->keep_alive = true;
        connection->verb = verb;
        connection->resource = StringLib::duplicate(resource);
        connection->data = StringLib::duplicate(data);
        connection->outq = new Publisher(outq_name);
        connection->client = lua_obj;

        /* Check if Blocking */
        Subscriber* inq = NULL;
        if(outq_name == NULL)
        {
            /* Create Subscriber */
            inq = new Subscriber(*(connection->outq));
        }

        /* Start Connection Thread */
        lua_obj->connMutex.lock();
        {
            connection->pid = new Thread(requestThread, connection);
            lua_obj->connections.add(connection);
        }
        lua_obj->connMutex.unlock();

        /* Check if Blocking */
        if(outq_name == NULL)
        {
            bool status = false;

            /* Setup Response Variables */
            int total_response_length = 0;
            List<in_place_rsps_t> responses;

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
                        in_place_rsps_t rsps = {(const char*)ref.data, ref.size};
                        responses.add(rsps);
                    }
                    else // terminator
                    {
                        done = true;
                        status = true;
                    }

                    /* Dereference Message */
                    inq->dereference(ref);
                }
                else if(recv_status == TIMEOUT_RC)
                {
                    attempts++;
                    if(attempts >= MAX_TIMEOUTS)
                    {
                        mlog(CRITICAL, "Maximum number of attempts to read queue reached\n");
                        done = true;
                    }
                }
                else
                {
                    mlog(CRITICAL, "Failed queue receive on %s with error %d", lua_obj->getName(), recv_status);
                    done = true;
                }
            }

            /* Delete Subscriber */
            if(inq) delete inq;

            /* Allocate and Build Response Array */
            char* response = new char [total_response_length + 1];
            List<in_place_rsps_t>::Iterator iterator(responses);
            int index = 0;
            for(int i = 0; i < responses.length(); i++)
            {
                LocalLib::copy(&response[index], iterator[i].data, iterator[i].size);
                index += iterator[i].size;
            }
            response[index] = '\0';
        
            /* Return Respnonse String */
            lua_pushlstring(L, response, total_response_length + 1);
            delete [] response;
            return returnLuaStatus(L, status, 2);
        }

        /* Return */
        return returnLuaStatus(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error initiating request: %s", e.what());
        return returnLuaStatus(L, false);
    }
}
