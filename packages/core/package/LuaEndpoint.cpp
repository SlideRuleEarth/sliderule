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

#include <algorithm>

#include "LuaEndpoint.h"
#include "OsApi.h"
#include "SystemConfig.h"
#include "TimeLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaEndpoint::LUA_META_NAME = "LuaEndpoint";
const struct luaL_Reg LuaEndpoint::LUA_META_TABLE[] = {
    {NULL,          NULL}
};

const char* LuaEndpoint::ENDPOINT_MAIN = "main";
const char* LuaEndpoint::ENDPOINT_LOGGING = "logging";
const char* LuaEndpoint::ENDPOINT_ROLES = "roles";
const char* LuaEndpoint::ENDPOINT_SIGNED = "signed";
const char* LuaEndpoint::ENDPOINT_OUTPUTS = "outputs";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - endpoint()
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaCreate (lua_State* L)
{
    try
    {
        /* Create Lua Endpoint */
        return createLuaObject(L, new LuaEndpoint(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * defaultHandler
 *----------------------------------------------------------------------------*/
void LuaEndpoint::defaultHandler (Request* request, LuaEngine* engine, content_t selected_output, const char* arguments)
{
    /* Get Lua State */
    lua_State* L = engine->getLuaState();

    /* Set Environment */
    if(selected_output == BINARY)
    {
        request->setLuaTable(L, request->id, request->rspq.getName(), arguments);
    }
    else
    {
        request->setLuaTable(L, request->id, "", arguments);
    }

    /* Get Main Function */
    lua_getfield(L, -1, ENDPOINT_MAIN);
    if(!lua_isfunction(L, -1))
    {
        FString error_msg("Did not find function <%s> to call in %s", ENDPOINT_MAIN, request->resource);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
    }

    /* Send Header for Binary Output */
    if(selected_output == BINARY)
    {
        sendHeader(OK, content2str(BINARY), &request->rspq, NULL, "chunked");
    }

    /* Execute Main Function */
    int lua_status = lua_pcall(L, 0, LUA_MULTRET, 0);
    lua_pop(L, 1);
    bool in_error = false;
    const char* result = engine->getResult(&in_error);

    /* Handle Text and JSON Output */
    if(selected_output == TEXT or selected_output == JSON)
    {
        if(result && !in_error)
        {
            sendHeader(OK, content2str(selected_output), &request->rspq, result);
        }
        else if(result)
        {
            sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, result);
        }
        else
        {
            FString error_msg("Endpoint %s returned no results", request->resource);
            sendHeader(Not_Found, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_RESOURCE_DOES_NOT_EXIST, "%s", error_msg.c_str());
        }
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::LuaEndpoint(lua_State* L):
    EndpointObject(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::~LuaEndpoint(void) = default;

/*----------------------------------------------------------------------------
 * handleRequest
 *----------------------------------------------------------------------------*/
void LuaEndpoint::handleRequest (Request* request)
{
    const Thread pid(requestThread, request, false);
}

/*----------------------------------------------------------------------------
 * loadLuaScript
 *----------------------------------------------------------------------------*/
void LuaEndpoint::loadLuaScript (Request* request, LuaEngine* engine, const char* script)
{
    const bool status = engine->execute(script, reinterpret_cast<const char*>(request->body));
    if(!status) // check status of loading script
    {
        FString error_msg("Failed to load script %s for request %s", script, request->id);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_FAILURE, "%s", error_msg.c_str());
    }

    if(!lua_istable(engine->getLuaState(), -1)) // check script properly returned an endpoint package
    {
        FString error_msg("Malformed endpoint in %s for request %s", script, request->id);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_FAILURE, "%s", error_msg.c_str());
    }
}

/*----------------------------------------------------------------------------
 * logRequest
 *----------------------------------------------------------------------------*/
void LuaEndpoint::logRequest (Request* request, lua_State* L)
{
    event_level_t log_level = INFO;
    lua_getfield(L, -1, ENDPOINT_LOGGING);
    if(lua_isinteger(L, -1))
    {
        log_level = static_cast<event_level_t>(lua_tointeger(L, -1));
    }
    else
    {
        mlog(WARNING, "Logging level for %s was not explicitly set", request->resource);
    }
    lua_pop(L, 1);
    mlog(log_level, "%s %s: %s", verb2str(request->verb), request->resource, request->body);

}

/*----------------------------------------------------------------------------
 * checkRole
 *----------------------------------------------------------------------------*/
void LuaEndpoint::checkRole (Request* request, lua_State* L)
{
    lua_getfield(L, -1, ENDPOINT_ROLES);
    if(lua_istable(L, -1))
    {
        const int tbl_size = lua_rawlen(L, -1);
        if(tbl_size > 0) // only perform checks if roles are supplied
        {
            /* Read Roles from Endpoint */
            vector<string> roles;
            for(int i = 0; i < tbl_size; i++)
            {
                lua_rawgeti(L, -1, i + 1);
                if(lua_isstring(L, -1))
                {
                    roles.emplace_back(lua_tostring(L, -1));
                }
                else
                {
                    mlog(WARNING, "Invalid entry encountered: %d, %d", i, lua_type(L, -1));
                }
                lua_pop(L, 1);
            }

            /* Get and Check Roles from Request */
            const char* org_roles_hdr = request->getHdrOrgRoles();
            List<string*>* user_roles = StringLib::split(org_roles_hdr, StringLib::size(org_roles_hdr), ' ');
            bool match_found = false;
            for(const string& role: roles)
            {
                for(int i = 0; i < user_roles->length(); i++)
                {
                    if(role == *user_roles->get(i))
                    {
                        match_found = true;
                        break;
                    }
                }
            }

            /* Handle No Matching Role Found */
            if(!match_found)
            {
                FString error_msg("User must be a member to execute this endpoint");
                sendHeader(Unauthorized, content2str(TEXT), &request->rspq, error_msg.c_str());
                throw RunTimeException(CRITICAL, RTE_UNAUTHORIZED, "%s", error_msg.c_str());
            }
        }
    }
    else
    {
        mlog(WARNING, "Organizational role requirement for %s was not explicitly set", request->id);
    }
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * checkSignature
 *----------------------------------------------------------------------------*/
void LuaEndpoint::checkSignature (Request* request, lua_State* L)
{
    lua_getfield(L, -1, ENDPOINT_SIGNED);
    if(lua_isboolean(L, -1))
    {
        const bool signature_required = lua_toboolean(L, -1);
        if(signature_required)
        {
            const bool is_signed = request->verifyHdrSignature(request->getHdrAccount());
            if(!is_signed)
            {
                FString error_msg("User must use a signed request for this endpoint");
                sendHeader(Unauthorized, content2str(TEXT), &request->rspq, error_msg.c_str());
                throw RunTimeException(CRITICAL, RTE_UNAUTHORIZED, "%s", error_msg.c_str());

            }
        }
    }
    else
    {
        mlog(WARNING, "Signature requirement for %s was not explicitly set", request->id);
    }
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * selectOutput
 *----------------------------------------------------------------------------*/
EndpointObject::content_t LuaEndpoint::selectOutput (Request* request, lua_State* L)
{
    /* Get Supported Outputs */
    vector<content_t> outputs;
    lua_getfield(L, -1, ENDPOINT_OUTPUTS);
    if(lua_istable(L, -1))
    {
        const int tbl_size = lua_rawlen(L, -1);
        if(tbl_size > 0)
        {
            /* Read Outputs from Endpoint */
            for(int i = 0; i < tbl_size; i++)
            {
                lua_rawgeti(L, -1, i + 1);
                if(lua_isstring(L, -1))
                {
                    const char* content_str = lua_tostring(L, -1);
                    content_t content = str2content(content_str);
                    if(content != UNKNOWN)
                    {
                        outputs.push_back(content);
                    }
                    else
                    {
                        mlog(WARNING, "Unrecognized output specified in %s: %s", request->resource, content_str);
                    }
                }
                else
                {
                    mlog(WARNING, "Invalid entry encountered: %d, %d", i, lua_type(L, -1));
                }
                lua_pop(L, 1);
            }
        }
    }
    else
    {
        mlog(WARNING, "Output for %s was not explicitly set", request->resource);
    }
    lua_pop(L, 1);

    /* Select Output */
    string* accept_hdr;
    if(request->headers.find("Accept", &accept_hdr)) // user requested output
    {
        content_t accepted_content = str2content(accept_hdr->c_str());
        if(accepted_content == UNKNOWN)
        {
            FString error_msg("Unsupported output: %s", accept_hdr->c_str());
            sendHeader(Not_Acceptable, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
        }
        else if(std::find(outputs.begin(), outputs.end(), accepted_content) == outputs.end())
        {
            FString error_msg("Endpoint %s does not support %s output", request->resource, content2str(accepted_content));
            sendHeader(Not_Acceptable, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, error_msg.c_str());
        }
        return accepted_content; // use requested output
    }
    else if(!outputs.empty()) // nothing was requested
    {
        return outputs.front(); // use first output provided by endpoint
    }
    else // nothing is provided or requested
    {
        return TEXT; // default output
    }
}

/*----------------------------------------------------------------------------
 * checkMemoryUsage
 *----------------------------------------------------------------------------*/
void LuaEndpoint::checkMemoryUsage(Request* request)
{
    const double memory_usage = OsApi::memusage();
    if(memory_usage >= SystemConfig::settings().memoryThreshold.value)
    {
        FString error_msg("Memory (%d%%) exceeded threshold, not performing request: %s", (int)(memory_usage * 100.0), request->resource);
        sendHeader(Service_Unavailable, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_NOT_ENOUGH_MEMORY, "%s", error_msg.c_str());
    }
}

/*----------------------------------------------------------------------------
 * executeEndpoint
 *----------------------------------------------------------------------------*/
void LuaEndpoint::executeEndpoint (Request* request, LuaEngine* engine, content_t selected_output, const char* arguments)
{
    handler_f handler = retrieveHandler(selected_output); // returns NULL if no handler is found
    if(handler)
    {
        handler(request, engine, selected_output, arguments);
    }
    else
    {
        FString error_msg("Unable to handle requested format: %s", content2str(selected_output));
        sendHeader(Method_Not_Implemented, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_FAILURE, "%s", error_msg.c_str());
    }
}

/*----------------------------------------------------------------------------
 * requestThread
 *----------------------------------------------------------------------------*/
void* LuaEndpoint::requestThread (void* parm)
{
    EndpointObject::Request* request = static_cast<EndpointObject::Request*>(parm);
    const double start = TimeLib::latchtime();
    int status_code = RTE_STATUS;

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, request->trace_id, "lua_endpoint", "{\"verb\":\"%s\", \"resource\":\"%s\"}", verb2str(request->verb), request->resource);

    /* Get Script and Arguments */
    const char* arguments = NULL;
    const char* script = LuaEngine::sanitize(request->resource, &arguments);

    /* Execute Lua Script */
    try
    {
        LuaEngine engine(trace_id, NULL); // TODO: implement lua hook that checks for the timeout to have expired
        loadLuaScript(request, &engine, script); // throws on error
        logRequest(request, engine.getLuaState()); // logs request
        checkRole(request, engine.getLuaState()); // throws on error
        checkSignature(request, engine.getLuaState()); // throws on error
        checkMemoryUsage(request); // throws on error
        content_t selected_output = selectOutput(request, engine.getLuaState()); // throws on error, returns output format
        executeEndpoint(request, &engine, selected_output); // executes registered handler, throws on error
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "%s", e.what());
        status_code = e.code();
    }

    /* End Response */
    const int rc = request->rspq.postCopy("", 0, SystemConfig::settings().publishTimeoutMs.value);
    if(rc <= 0) mlog(CRITICAL, "Failed to post terminator on %s: %d", request->rspq.getName(), rc);

    /* Generate Telemetry */
    const EventLib::tlm_input_t tlm = {
        .code = status_code,
        .duration = static_cast<float>(TimeLib::latchtime() - start),
        .source_ip = request->getHdrSourceIp(),
        .endpoint = request->resource,
        .client = request->getHdrClient(),
        .account = request->getHdrAccount()
    };
    telemeter(INFO, tlm);

    /* Clean Up */
    delete request;
    delete [] script;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
