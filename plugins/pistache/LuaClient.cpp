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

#include "LuaClient.h"
#include "RouteHandler.h"
#include "core.h"

using namespace Pistache;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaClient::LuaMetaName = "LuaClient";
const struct luaL_Reg LuaClient::LuaMetaTable[] = {
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
int LuaClient::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* outq_name   = getLuaString(L, 1, true, NULL);
        long        num_threads = getLuaInteger(L, 2, true, 1);

        /* Create Lua Endpoint */
        return createLuaObject(L, new LuaClient(L, outq_name, num_threads));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaClient::LuaClient(lua_State* L,  const char* outq_name, size_t num_threads):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    numThreads(num_threads)
{
    /* Create Output Queue */
    outQ = NULL;
    if(outq_name)
    {
        outQ = new Publisher(outq_name);
    }

    /* Set Number of Threads */
    auto opts = Http::Client::options().threads(1).maxConnectionsPerHost(num_threads);
    client.init(opts);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaClient::~LuaClient(void)
{
    if(outQ) delete outQ;
    client.shutdown();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaRoute - :request(<action>, <url>, [<body>], [<timeout>])
 *----------------------------------------------------------------------------*/
int LuaClient::luaRequest(lua_State* L)
{
    bool status = false;
    bool in_error = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        LuaClient* lua_obj = (LuaClient*)getLuaSelf(L, 1);

        /* Get Action */
        LuaEndpoint::verb_t action = LuaEndpoint::INVALID;
        if(lua_isnumber(L, 2))
        {
            action = (LuaEndpoint::verb_t)getLuaInteger(L, 2);
        }
        else
        {
            const char* action_str = getLuaString(L, 2);
            action = LuaEndpoint::str2verb(action_str);
        }

        /* Check Action */
        if(action != LuaEndpoint::GET && action != LuaEndpoint::POST && action != LuaEndpoint::PUT)
        {
            throw LuaException("Invalid action: %d", action);
        }

        /* Get URL */
        const char* url = getLuaString(L, 3);

        /* Get Body */
        bool body_provided = false;
        const char* body = getLuaString(L, 4, true, NULL, &body_provided);
        if(body_provided && action == LuaEndpoint::GET)
        {
            mlog(WARNING, "Body ignored for GET requests\n");
        }

        /* Get Timeout */
        bool timeout_provided = false;
        int timeout = getLuaInteger(L, 5, true, SYS_TIMEOUT, &timeout_provided);
        if(timeout_provided && lua_obj->outQ)
        {
            mlog(WARNING, "Timeout ignored for asynchronous clients\n");
        }

        /* Make Request */
        if(action == LuaEndpoint::GET)
        {
            auto resp = lua_obj->client.get(url).send();
            status = true;
        }
        else if(action == LuaEndpoint::POST)
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
                                    mlog(ERROR, "Failed to get respone on post to %s\n", url);
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
                                mlog(CRITICAL, "Failed to get response on post to %s: %s\n", url, e.what());
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
                        mlog(CRITICAL, "Timeout on response on post to %s\n", url);
                    }

                }
                lua_obj->requestSignal.unlock();
            }
        }
        else if(action == LuaEndpoint::PUT)
        {
            auto resp = lua_obj->client.put(url).body(body).send();
            status = true;
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error making request: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_obj_to_return);
}
