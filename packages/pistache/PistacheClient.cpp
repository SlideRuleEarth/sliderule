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

#include "PistacheClient.h"
#include "RouteHandler.h"
#include "core.h"

using namespace Pistache;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* PistacheClient::LuaMetaName = "PistacheClient";
const struct luaL_Reg PistacheClient::LuaMetaTable[] = {
    {"request",     luaRequest},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - client([<output stream>], [<number of threads>])
 *
 *  If an output stream is provided, then the client is asynchronous and will
 *  post all responses to the provided stream.
 *
 *  If no output stream is provided, then the client will block on each request
 *  and return each response directly back to Lua.
 *----------------------------------------------------------------------------*/
int PistacheClient::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name   = getLuaString(L, 1, true, NULL);
        long        num_threads = getLuaInteger(L, 2, true, 1);

        /* Create Lua Endpoint */
        return createLuaObject(L, new PistacheClient(L, outq_name, num_threads));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PistacheClient::PistacheClient(lua_State* L,  const char* outq_name, size_t num_threads):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    /* Create Output Queue */
    outQ = NULL;
    if(outq_name)
    {
        outQ = new Publisher(outq_name);
    }

    /* Set Number of Threads */
    auto opts = Http::Experimental::Client::options().threads(num_threads).maxConnectionsPerHost(8);
    client.init(opts);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PistacheClient::~PistacheClient(void)
{
    if(outQ) delete outQ;

    mlog(CRITICAL, "Shutting down HTTP client %s", getName());
    client.shutdown();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaRoute - :request(<action>, <url>, [<body>], [<timeout>])
 *----------------------------------------------------------------------------*/
int PistacheClient::luaRequest(lua_State* L)
{
    bool status = false;
    bool in_error = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        PistacheClient* lua_obj = (PistacheClient*)getLuaSelf(L, 1);

        /* Get Action */
        PistacheServer::verb_t action = PistacheServer::INVALID;
        if(lua_isnumber(L, 2))
        {
            action = (PistacheServer::verb_t)getLuaInteger(L, 2);
        }
        else
        {
            const char* action_str = getLuaString(L, 2);
            action = PistacheServer::str2verb(action_str);
        }

        /* Check Action */
        if(action != PistacheServer::GET && action != PistacheServer::POST && action != PistacheServer::PUT)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid action: %d", action);
        }

        /* Get URL */
        const char* url = getLuaString(L, 3);

        /* Get Body */
        bool body_provided = false;
        const char* body = getLuaString(L, 4, true, NULL, &body_provided);
        if(body_provided && action == PistacheServer::GET)
        {
            mlog(WARNING, "Body ignored for GET requests");
        }

        /* Get Timeout */
        bool timeout_provided = false;
        int timeout = getLuaInteger(L, 5, true, SYS_TIMEOUT, &timeout_provided);
        if(timeout_provided && lua_obj->outQ)
        {
            mlog(WARNING, "Timeout ignored for asynchronous clients");
        }

        /* Make Request */
        if(action == PistacheServer::GET)
        {
            auto resp = lua_obj->client.get(url).send();
            status = true;
        }
        else if(action == PistacheServer::POST)
        {
            SafeString lua_result;
            auto resp = lua_obj->client.post(url).body(body).send();
            resp.then(  [&](Http::Response response)
                        {
                            auto response_body = response.body();
                            if(lua_obj->outQ)
                            {
                                /* Asynchronously Post Response */
                                if(!response_body.empty())
                                {
                                    lua_obj->outQ->postString("%s", response_body.c_str());
                                }
                                else if(response.code() != Http::Code::Ok)
                                {
                                    mlog(ERROR, "Failed to get respone on post to %s", url);
                                }

                                /* Assume Success */
                                status = true;
                            }
                            else
                            {
                                /* Save Off and Signal Response */
                                lua_obj->requestSignal.lock();
                                {
                                    lua_result += response_body.c_str();
                                    lua_obj->requestSignal.signal(REQUEST_SIGNAL);
                                }
                                lua_obj->requestSignal.unlock();
                            }
                        },
                        [&](std::exception_ptr exc)
                        {
                            try { std::rethrow_exception(exc); }
                            catch (const std::exception &e)
                            {
                                mlog(CRITICAL, "Failed to get response on post to %s: %s", url, e.what());
                                in_error = true;
                            }
                        });

            /* Block on Synchronous Response */
            if(!lua_obj->outQ)
            {
                lua_obj->requestSignal.lock();
                {
                    if(lua_obj->requestSignal.wait(REQUEST_SIGNAL, timeout))
                    {
                        lua_pushlstring(L, lua_result.getString(), lua_result.getLength());
                        num_obj_to_return = 2;
                        status = true;
                    }
                    else if(!in_error)
                    {
                        mlog(CRITICAL, "Timeout on response on post to %s", url);
                    }

                }
                lua_obj->requestSignal.unlock();
            }
        }
        else if(action == PistacheServer::PUT)
        {
            auto resp = lua_obj->client.put(url).body(body).send();
            status = true;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error making request: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
