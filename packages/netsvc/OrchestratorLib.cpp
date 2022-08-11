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
    SafeString rqst("{\"service\":\"%s\", \"lifetime\": %d, \"name\": \"%s\"}", service, lifetime, name);

    HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::POST, "/discovery/", rqst.getString(), false, NULL);
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

                int64_t exp_unix_ms = (expiration * 1000);
                int64_t exp_gps_ms = TIME_UNIX_TO_GPS(exp_unix_ms);
                TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(exp_gps_ms);
                TimeLib::date_t date = TimeLib::gmt2date(gmt);
                mlog(INFO, "Registered to <%s> until %d/%d/%d %02d:%02d:%02d\n", membership, date.month, date.day, date.year, gmt.hour, gmt.minute, gmt.second);
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
 * lock
 *----------------------------------------------------------------------------*/
OrchestratorLib::nodes_t* OrchestratorLib::lock (const char* service, int nodes_needed, int timeout_secs, bool verbose)
{
    nodes_t* nodes = new nodes_t;

    HttpClient orchestrator(NULL, URL);
    SafeString rqst("{\"service\":\"%s\", \"nodesNeeded\": %d, \"timeout\": %d}", service, nodes_needed, timeout_secs);

    HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::GET, "/discovery/lock", rqst.getString(), false, NULL);
    if(rsps.code == EndpointObject::OK)
    {
        rapidjson::Document json;
        json.Parse(rsps.response);

        for(rapidjson::SizeType i = 0; i < json["members"].Size(); i++)
        {
            const char* name = StringLib::duplicate(json["members"][i].GetString());
            nodes->members.add(name);
        }

        for(rapidjson::SizeType i = 0; i < json["transactions"].Size(); i++)
        {
            double transaction = json["transactions"][i].GetDouble();
            nodes->transactions.add(transaction);
        }

        if(verbose)
        {
            for(int i = 0; i < nodes->members.length() && i < nodes->transactions.length(); i++)
            {
                mlog(INFO, "Locked - %s <%ld>", nodes->members[i], nodes->transactions[i]);
            }

            if(nodes->members.length() != nodes->transactions.length())
            {
                mlog(CRITICAL, "Missing information from locked response; %d members != %d transactions", nodes->members.length(), nodes->transactions.length());
            }
        }
    }
    else
    {
        mlog(CRITICAL, "Failed to lock nodes on %s", service);
    }

    return nodes;
}

/*----------------------------------------------------------------------------
 * lock
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::unlock (long transactions[], int num_transactions, bool verbose)
{
    assert(num_transactions > 0);

    bool status = true;

    HttpClient orchestrator(NULL, URL);
    SafeString rqst("{\"transactions\": [%ld", transactions[0]);
    char txstrbuf[64];
    for(int t = 1; t < num_transactions; t++) rqst += StringLib::format(txstrbuf, 64, ",%ld", transactions[t]);
    rqst += "]}";

    HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::GET, "/discovery/unlock", rqst.getString(), false, NULL);
    if(rsps.code == EndpointObject::OK)
    {
        if(verbose)
        {
            rapidjson::Document json;
            json.Parse(rsps.response);

            int completed = json["complete"].GetInt();
            int failed = json["fail"].GetInt();

            mlog(INFO, "Completed %d transactions%s", completed, failed ? " with failures" : " successfully");
        }
    }
    else
    {
        mlog(CRITICAL, "Failed to unlock %d transactions", num_transactions);
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
 * luaLock - orchlock(<host>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaLock(lua_State* L)
{
    nodes_t* nodes = NULL;
    try
    {
        const char* service = LuaObject::getLuaString(L, 1);
        int nodes_needed    = LuaObject::getLuaInteger(L, 2);
        int timeout_secs    = LuaObject::getLuaInteger(L, 3);
        bool verbose        = LuaObject::getLuaBoolean(L, 4, true, false);

        nodes = lock(service, nodes_needed, timeout_secs, verbose);

        lua_newtable(L);
        for(int i = 0; i < nodes->transactions.length(); i++)
        {
            SafeString txidstr("%ld", nodes->transactions[i]);
            LuaEngine::setAttrStr(L, txidstr.getString(), nodes->members[i]);
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error locking members: %s", e.what());
        lua_pushnil(L);
    }

    if(nodes) delete nodes;

    return 1;
}

/*----------------------------------------------------------------------------
 * luaHealth - orchhealth()
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
