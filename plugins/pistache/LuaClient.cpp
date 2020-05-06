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
 * luaRoute - :request(<action>, <url>, [<body>])
 *----------------------------------------------------------------------------*/
int LuaClient::luaRequest(lua_State* L)
{
    bool status = false;

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
        const char* url = getLuaString(L, 2);

        /* Get Parameters */
        const char* body = getLuaString(L, 3, true, NULL);

        /* Make Request */
        if(action == LuaEndpoint::GET)
        {
            auto resp = lua_obj->client.get(url).send();
            status = true;
        }
        else if(action == LuaEndpoint::POST)
        {
            auto resp = lua_obj->client.post(url).body(body).send();
            status = true;
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
    return returnLuaStatus(L, status);
}
