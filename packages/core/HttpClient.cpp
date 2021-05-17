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
        int         port            = (int)getLuaInteger(L, 1);
        const char* ip_addr         = getLuaString(L, 2, true, NULL);

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

// GET / HTTP/1.1
// Host: 
// User-Agent: sliderule/x.y.z
// Accept: */*
// Content-Length:

    delete connection->outq;

    return NULL;
}


/*----------------------------------------------------------------------------
 * luaAttach - :request(<verb>, <data>, [<outq>])
 *----------------------------------------------------------------------------*/
int HttpClient::luaRequest (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        HttpClient* lua_obj = (HttpClient*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* verb        = getLuaString(L, 2);
        const char* data        = getLuaString(L, 3);
        const char* outq_name   = getLuaString(L, 4, true, NULL);

        /* Allocate Connection Structure */
        connection_t* connection = new connection_t;

        /* Create Output Queue */
        connection->outq = new Publisher(outq_name);

        /* Check if Non-Blocking */
        if(outq_name)
        {
            /* Start Connection Thread */
            lua_obj->connMutex.lock();
            {
                connection->active = true;
                connection->pid = new Thread(requestThread, connection);
                lua_obj->connections.add(connection);
            }
            lua_obj->connMutex.unlock();
        }
        else
        {
            /* Setup Subscriber and Make Request */
            Subscriber inq(*(connection->outq));
            requestThread((void*)connection);

            /* Setup Response Variables */
            int total_response_length = 0;
            List<in_place_rsps_t> responses;

            /* Read Responses */
            bool done = false;
            while(!done)
            {
                Subscriber::msgRef_t ref;
                int recv_status = inq.receiveRef(ref, SYS_TIMEOUT);
                if(recv_status > 0)
                {
                    if(ref.size > 0)
                    {
                        total_response_length += ref.size;
                        in_place_rsps_t rsps = {(const char*)ref.data, ref.size};
                        responses.add(rsps);
                    }
                    else
                    {
                        done = true;
                    }

                    /* Dereference Message */
                    inq.dereference(ref);
                }
                else
                {
                    /* Given that this code is executing after the requestThread function completes,
                     * there is no valid reason to timeout or have any other error on the queue; the
                     * queue should have the termination message which is the valid exit from this loop. */
                    mlog(CRITICAL, "Failed queue receive on %s with error %d", lua_obj->getName(), recv_status);
                    done = false;
                }
            }

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
        
            /* Return String */
            lua_pushlstring(L, response, total_response_length + 1);
            delete [] response;
            return returnLuaStatus(L, false, 2);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error initiating request: %s", e.what());
        return returnLuaStatus(L, false);
    }
}
