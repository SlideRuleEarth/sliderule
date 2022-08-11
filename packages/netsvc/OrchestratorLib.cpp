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


#include "OrchestratorLib.h"
#include "core.h"

#include <rapidjson/document.h>

/******************************************************************************
 * ORCHESTRATOR LIBRARY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Static Data
 *----------------------------------------------------------------------------*/

const char* OrchestratorLib::URL = NULL;

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void OrchestratorLib::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void OrchestratorLib::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * registerService
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::registerService (const char* service, int lifetime, const char* name, bool verbose)
{
    bool status = true;

    HttpClient orchestrator(NULL, URL);
    SafeString orch_rqst_data("{\"service\":\"%s\", \"lifetime\": %d, \"name\": \"%s\"}", service, lifetime, name);
    HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::POST, "/discovery/", orch_rqst_data.getString(), false, NULL);
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            if(verbose)
            {
                rapidjson::Document json;
                json.Parse(rsps.response);

                const char* membership = json[name][0].GetString();
                double expiration = json[name][1].GetDouble();

                TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(expiration * 1000);
                TimeLib::date_t date = TimeLib::gmt2date(gmt);
                mlog(INFO, "Registered to <%s> until %d/%d/%d %d:%d:%d\n", membership, date.day, date.month, date.year, gmt.hour, gmt.minute, gmt.second);
            }
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Failed process response to registration: %s", rsps.response);
            status = false;
        }
    }
    else
    {
        mlog(CRITICAL, "Failed to register %s to %s", name, service);
        status = false;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * health
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::health (void)
{
    bool status = false;

    HttpClient orchestrator(NULL, URL);
    HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::GET, "/discovery/health", NULL, false, NULL);
    if(rsps.code == EndpointObject::OK)
    {
        rapidjson::Document json;
        json.Parse(rsps.response);

        rapidjson::Value& s = json["health"];
        status = s.GetBool();
    }

    return status;
}

/*----------------------------------------------------------------------------
 * lock
 *----------------------------------------------------------------------------*/
OrchestratorLib::nodes_t* OrchestratorLib::lock (const char* service, int nodes_needed, int timeout_secs)
{
    nodes_t* nodes = new nodes_t;

    HttpClient orchestrator(NULL, URL);
    SafeString orch_rqst_data("{\"service\":\"%s\", \"nodesNeeded\": %d, \"timeout\": %d}", service, nodes_needed, timeout_secs);
    HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::GET, "/discovery/lock", orch_rqst_data.getString(), false, NULL);

    if(rsps.code == EndpointObject::OK)
    {
        print2term("RESPONSE: %s\n", rsps.response);

        rapidjson::Document json;
        json.Parse(rsps.response);

        rapidjson::Value& s = json["members"];
        print2term("IS ARRAY: %d\n", s.IsArray());
    }

    return nodes;
}

/*----------------------------------------------------------------------------
 * luaSetUrl - orchurl(<URL>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaSetUrl(lua_State* L)
{
    try
    {
        const char* _url = LuaObject::getLuaString(L, 1);

        if(URL) delete [] URL;
        URL = StringLib::duplicate(_url);

        lua_pushboolean(L, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting URL: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaRegisterService - orchreg(<service>, <lifetime>, <name>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaRegisterService(lua_State* L)
{
    bool status = false;
    try
    {
        const char* service = LuaObject::getLuaString(L, 1);
        int lifetime        = LuaObject::getLuaInteger(L, 2);
        const char* name    = LuaObject::getLuaString(L, 3);
        bool verbose        = LuaObject::getLuaBoolean(L, 4, true, false);

        status = registerService(service, lifetime, name, verbose);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error registering: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaHealth - orchhelath()
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaHealth(lua_State* L)
{
    try
    {
        lua_pushboolean(L, health());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting health: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaLock - orchlock(<host>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaLock(lua_State* L)
{
    try
    {
        const char* service = LuaObject::getLuaString(L, 1);
        int nodes_needed    = LuaObject::getLuaInteger(L, 2);
        int timeout_secs    = LuaObject::getLuaInteger(L, 3);

        nodes_t* nodes = lock(service, nodes_needed, timeout_secs);

        lua_newtable(L);
        for(int i = 0; i < nodes->txids.length(); i++)
        {
            SafeString txidstr("%ld", nodes->txids[i]);
            LuaEngine::setAttrStr(L, txidstr.getString(), nodes->members[i]);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error locking members: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

