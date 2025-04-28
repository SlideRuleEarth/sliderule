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

#include <rapidjson/document.h>

#include "OrchestratorLib.h"
#include "OsApi.h"
#include "TimeLib.h"
#include "LuaEngine.h"
#include "EndpointObject.h"
#include "CurlLib.h"

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
    URL = StringLib::duplicate("http://127.0.0.1:8050");
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void OrchestratorLib::deinit (void)
{
    delete [] URL;
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
OrchestratorLib::rsps_t OrchestratorLib::request (EndpointObject::verb_t verb, const char* resource, const char* data)
{
    rsps_t rsps;
    const FString path("%s%s", URL, resource);
    rsps.code = CurlLib::request(verb, path.c_str(), data, &rsps.response, &rsps.size);
    return rsps;
}

/*----------------------------------------------------------------------------
 * registerService
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::registerService (const char* service, int lifetime, const char* address, bool initial_registration, bool verbose)
{
    bool status = true;

    const FString rqst("{\"service\":\"%s\", \"lifetime\": %d, \"address\": \"%s\", \"reset\": %s}", service, lifetime, address, initial_registration ? "true" : "false");
    const rsps_t rsps = request(EndpointObject::POST, "/discovery/register", rqst.c_str());
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            if(verbose)
            {
                rapidjson::Document json;
                json.Parse(rsps.response);

                const char* membership = json[address][0].GetString();
                const double expiration = json[address][1].GetDouble();

                const int64_t exp_unix_us = (expiration * 1000000);
                const int64_t exp_gps_ms = TimeLib::sys2gpstime(exp_unix_us);
                const TimeLib::gmt_time_t gmt = TimeLib::gps2gmttime(exp_gps_ms);
                const TimeLib::date_t date = TimeLib::gmt2date(gmt);
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
        mlog(CRITICAL, "Failed to register %s to %s", address, service);
        status = false;
    }

    delete [] rsps.response;

    return status;
}

/*----------------------------------------------------------------------------
 * selflock
 *----------------------------------------------------------------------------*/
long OrchestratorLib::selflock (const char* service, int timeout_secs, int locks_per_node, bool verbose)
{
    long transaction = INVALID_TX_ID;
    const FString rqst("{\"service\":\"%s\", \"address\": \"http://%s:9081\", \"timeout\": %d, \"locksPerNode\": %d}", service, SockLib::sockipv4(), timeout_secs, locks_per_node);
    const rsps_t rsps = request(EndpointObject::POST, "/discovery/selflock", rqst.c_str());
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            rapidjson::Document json;
            json.Parse(rsps.response);

            transaction = json["transaction"].GetDouble();
            if(verbose)
            {
                mlog(INFO, "Locked Self <%ld>", transaction);
            }
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Failed process response to lock self: %s", rsps.response);
        }
    }
    else
    {
        mlog(CRITICAL, "Encountered HTTP error <%ld> when locking self on %s", rsps.code, service);
    }

    delete [] rsps.response;

    return transaction;
}

/*----------------------------------------------------------------------------
 * lock
 *----------------------------------------------------------------------------*/
vector<OrchestratorLib::Node*>* OrchestratorLib::lock (const char* service, int nodes_needed, int timeout_secs, int locks_per_node, bool verbose)
{
    vector<Node*>* nodes = NULL;

    const FString rqst("{\"service\":\"%s\", \"nodesNeeded\": %d, \"timeout\": %d, \"locksPerNode\": %d}", service, nodes_needed, timeout_secs, locks_per_node);
    const rsps_t rsps = request(EndpointObject::POST, "/discovery/lock", rqst.c_str());
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            rapidjson::Document json;
            json.Parse(rsps.response);

            const unsigned int num_members = json["members"].Size();
            const unsigned int num_transactions = json["transactions"].Size();
            if(num_members == num_transactions)
            {
                nodes = new vector<Node*>; // allocate node list to be returned
                for(rapidjson::SizeType i = 0; i < num_members; i++)
                {
                    const char* name = json["members"][i].GetString();
                    const double transaction = json["transactions"][i].GetDouble();
                    Node* node = new Node(name, transaction);
                    nodes->push_back(node);
                }
            }
            else
            {
                mlog(CRITICAL, "Missing information from locked response; %d members != %d transactions", num_members, num_transactions);
            }

            if(verbose && nodes)
            {
                for(unsigned i = 0; i < nodes->size(); i++)
                {
                    mlog(INFO, "Locked - %s <%ld>", nodes->at(i)->member, nodes->at(i)->transaction);
                }
            }
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Failed process response to lock: %s", rsps.response);
            if(nodes)
            {
                for(unsigned i = 0; i < nodes->size(); i++)
                {
                    delete nodes->at(i);
                }
                delete nodes;
                nodes = NULL;
            }
        }
    }
    else
    {
        mlog(CRITICAL, "Encountered HTTP error <%ld> when locking nodes on %s", rsps.code, service);
    }

    delete [] rsps.response;

    return nodes;
}

/*----------------------------------------------------------------------------
 * unlock
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::unlock (long transactions[], int num_transactions, bool verbose)
{
    assert(num_transactions > 0);

    bool status = true;

    char strbuf[64];
    string rqst;
    rqst += StringLib::format(strbuf, 64, "{\"transactions\": [%ld", transactions[0]);
    for(int t = 1; t < num_transactions; t++) rqst += StringLib::format(strbuf, 64, ",%ld", transactions[t]);
    rqst += "]}";

    const rsps_t rsps = request(EndpointObject::POST, "/discovery/unlock", rqst.c_str());
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            if(verbose)
            {
                rapidjson::Document json;
                json.Parse(rsps.response);

                const int completed = json["complete"].GetInt();
                const int failed = json["fail"].GetInt();

                mlog(INFO, "Completed %d transactions%s", completed, failed ? " with failures" : " successfully");
            }
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Failed process response to unlock: %s", rsps.response);
        }
    }
    else
    {
        mlog(CRITICAL, "Failed to unlock %d transactions", num_transactions);
        status = false;
    }

    delete [] rsps.response;

    return status;
}

/*----------------------------------------------------------------------------
 * health
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::health (void)
{
    bool status = false;

    const rsps_t rsps = request(EndpointObject::GET, "/discovery/health", NULL);
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            rapidjson::Document json;
            json.Parse(rsps.response);

            const rapidjson::Value& s = json["health"];
            status = s.GetBool();
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Failed to process response from health: %s", rsps.response);
        }
    }

    delete [] rsps.response;

    return status;
}

/*----------------------------------------------------------------------------
 * metric
 *----------------------------------------------------------------------------*/
bool OrchestratorLib::metric (const char* name, double value)
{
    bool status = false;

    FString data("{\"name\":\"%s\",\"value\":\"%lf\"}", name, value);

    const rsps_t rsps = request(EndpointObject::POST, "/discovery/metric", data.c_str());
    if(rsps.code == EndpointObject::OK)
    {
        status = true;
    }

    delete [] rsps.response;

    return status;
}

/*----------------------------------------------------------------------------
 * getNodes
 *----------------------------------------------------------------------------*/
int OrchestratorLib::getNodes (void)
{
    int num_nodes = 0;

    const char* data = "{\"service\":\"sliderule\"}";
    const rsps_t rsps = request(EndpointObject::GET, "/discovery/status", data);
    if(rsps.code == EndpointObject::OK)
    {
        try
        {
            rapidjson::Document json;
            json.Parse(rsps.response);

            const rapidjson::Value& s = json["nodes"];
            num_nodes = s.GetInt();
        }
        catch(const std::exception& e)
        {
            mlog(CRITICAL, "Failed to process response from status: %s", rsps.response);
        }
    }
    else
    {
        mlog(CRITICAL, "Failed to get discovery status: %ld", rsps.code);
    }

    delete [] rsps.response;

    return num_nodes;
}

/*----------------------------------------------------------------------------
 * luaUrl - orchurl(<URL>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaUrl(lua_State* L)
{
    try
    {
        const char* _url = LuaObject::getLuaString(L, 1);

        delete [] URL;
        URL = StringLib::duplicate(_url);
    }
    catch(const RunTimeException& e)
    {
        // silently fail... allows calling lua script to set nil
        // as way of keeping and returning the current value
        (void)e;
    }

    lua_pushstring(L, URL);

    return 1;
}

/*----------------------------------------------------------------------------
 * luaRegisterService - orchreg(<service>, <lifetime>, <address>, <initial_registration>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaRegisterService(lua_State* L)
{
    bool status = false;
    try
    {
        const char* service             = LuaObject::getLuaString(L, 1);
        const int lifetime              = LuaObject::getLuaInteger(L, 2);
        const char* address             = LuaObject::getLuaString(L, 3);
        const bool initial_registration = LuaObject::getLuaBoolean(L, 4);
        const bool verbose              = LuaObject::getLuaBoolean(L, 5, true, false);

        status = registerService(service, lifetime, address, initial_registration, verbose);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error registering: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaSelfLock - orchselflock(<service>, <timeout>, [<locks_per_node>])
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaSelfLock(lua_State* L)
{
    try
    {
        const char* service         = LuaObject::getLuaString(L, 1);
        const int timeout_secs      = LuaObject::getLuaInteger(L, 2);
        const int locks_per_node    = LuaObject::getLuaInteger(L, 3, true, 1);
        const bool verbose          = LuaObject::getLuaBoolean(L, 4, true, false);

        const long tx_id = selflock(service, timeout_secs, locks_per_node, verbose);
        lua_pushinteger(L, tx_id);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error locking members: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaLock - orchlock(<service>, <nodes_needed>, <timeout>, [<locks_per_node>])
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaLock(lua_State* L)
{
    vector<Node*>* nodes = NULL;
    try
    {
        const char* service         = LuaObject::getLuaString(L, 1);
        const int nodes_needed      = LuaObject::getLuaInteger(L, 2);
        const int timeout_secs      = LuaObject::getLuaInteger(L, 3);
        const int locks_per_node    = LuaObject::getLuaInteger(L, 4, true, 1);
        const bool verbose          = LuaObject::getLuaBoolean(L, 5, true, false);

        nodes = lock(service, nodes_needed, timeout_secs, locks_per_node, verbose);

        lua_newtable(L);
        for(unsigned i = 0; i < nodes->size(); i++)
        {
            const FString txidstr("%ld", nodes->at(i)->transaction);
            LuaEngine::setAttrStr(L, txidstr.c_str(), nodes->at(i)->member);
            delete nodes->at(i); // free node after using it
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error locking members: %s", e.what());
        lua_pushnil(L);
    }

    delete nodes;

    return 1;
}

/*----------------------------------------------------------------------------
 * luaUnlock - orchunlock(<[<txid>, <txid>, ...]>)
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaUnlock(lua_State* L)
{
    try
    {
        const bool verbose = LuaObject::getLuaBoolean(L, 2, true, false);

        if(!lua_istable(L, 1))
        {
            throw RunTimeException(CRITICAL, RTE_FAILURE, "must supply table for parameter #1");
        }

        const int num_transactions = lua_rawlen(L, 1);
        if(num_transactions > 0)
        {
            long* transactions = new long [num_transactions];
            for(int t = 0; t < num_transactions; t++)
            {
                lua_rawgeti(L, 1, t+1);
                transactions[t] = LuaObject::getLuaInteger(L, -1);
                lua_pop(L, 1);
            }

            const bool status = unlock(transactions, num_transactions, verbose);
            lua_pushboolean(L, status);

            delete [] transactions;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error unlocking transactions: %s", e.what());
        lua_pushnil(L);
    }

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

/*----------------------------------------------------------------------------
 * luaGetNodes - orchnodes()
 *----------------------------------------------------------------------------*/
int OrchestratorLib::luaGetNodes(lua_State* L)
{
    try
    {
        lua_pushinteger(L, getNodes());
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting number of nodes: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}
