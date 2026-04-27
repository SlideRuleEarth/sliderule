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
#include "RequestFields.h"

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
const char* LuaEndpoint::ENDPOINT_PARMS = "parms";

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
        const FString error_msg("Did not find function <%s> to call in %s", ENDPOINT_MAIN, request->resource);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
    }

    /* Send Header for Binary Output */
    if(selected_output == BINARY)
    {
        sendHeader(OK, content2str(BINARY), &request->rspq, NULL, true);
    }

    /* Execute Main Function */
    const int lua_status = lua_pcall(L, 0, LUA_MULTRET, 0); // removes function from stack
    bool in_error = false;
    const char* result = engine->getResult(&in_error, 1);

    /* Handle Text and JSON Output */
    if(selected_output == TEXT or selected_output == JSON)
    {
        if(lua_status != LUA_OK)
        {
            const FString error_msg("Endpoint %s encountered error: %s", request->resource, result);
            sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
        }
        else if(!result)
        {
            const FString error_msg("Endpoint %s returned no results", request->resource);
            sendHeader(Not_Found, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_RESOURCE_DOES_NOT_EXIST, "%s", error_msg.c_str());
        }
        else if(in_error)
        {
            sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, result);
        }
        else
        {
            sendHeader(OK, content2str(selected_output), &request->rspq, result);
        }
    }
    else if(lua_status != LUA_OK)
    {
        const FString error_msg("Endpoint %s encountered error: %s", request->resource, result);
        throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
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
LuaEndpoint::endpoint_t LuaEndpoint::loadLuaScript (Request* request, LuaEngine* engine, const string& script)
{
    endpoint_t endpoint;
    lua_State* L = engine->getLuaState();

    // execute script
    const bool status = engine->execute(script.c_str(), reinterpret_cast<const char*>(request->body));
    if(!status) // check status of loading script
    {
        const FString error_msg("Failed to load script %s for request %s", script.c_str(), request->id);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_FAILURE, "%s", error_msg.c_str());
    }

    if(!lua_istable(engine->getLuaState(), -1)) // check script properly returned an endpoint package
    {
        const FString error_msg("Malformed endpoint in %s for request %s", script.c_str(), request->id);
        sendHeader(Internal_Server_Error, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_FAILURE, "%s", error_msg.c_str());
    }

    // get log level
    lua_getfield(L, -1, ENDPOINT_LOGGING);
    if(lua_isinteger(L, -1))
    {
        endpoint.log_level = static_cast<event_level_t>(lua_tointeger(L, -1));
    }
    else
    {
        mlog(WARNING, "Logging level for %s was not explicitly set", request->resource);
    }
    lua_pop(L, 1);

    // get supported roles
    lua_getfield(L, -1, ENDPOINT_ROLES);
    if(lua_istable(L, -1))
    {
        const int tbl_size = lua_rawlen(L, -1);
        for(int i = 0; i < tbl_size; i++)
        {
            lua_rawgeti(L, -1, i + 1);
            if(lua_isstring(L, -1))
            {
                endpoint.allowed_roles.emplace_back(lua_tostring(L, -1));
            }
            else
            {
                mlog(WARNING, "Invalid entry encountered: %d, %d", i, lua_type(L, -1));
            }
            lua_pop(L, 1);
        }
    }
    else
    {
        mlog(WARNING, "Organizational role requirement for %s was not explicitly set", request->id);
    }
    lua_pop(L, 1);

    // get signature required
    lua_getfield(L, -1, ENDPOINT_SIGNED);
    if(lua_isboolean(L, -1))
    {
        endpoint.signature_required = lua_toboolean(L, -1);
    }
    else
    {
        mlog(WARNING, "Signature requirement for %s was not explicitly set", request->id);
    }
    lua_pop(L, 1);

    // get supported outputs
    lua_getfield(L, -1, ENDPOINT_OUTPUTS);
    if(lua_istable(L, -1))
    {
        const int tbl_size = lua_rawlen(L, -1);
        for(int i = 0; i < tbl_size; i++)
        {
            lua_rawgeti(L, -1, i + 1);
            if(lua_isstring(L, -1))
            {
                const char* content_str = lua_tostring(L, -1);
                const content_t content = str2content(content_str);
                if(content != UNKNOWN)
                {
                    endpoint.supported_outputs.push_back(content);
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
    else
    {
        mlog(WARNING, "Output for %s was not explicitly set", request->resource);
    }
    lua_pop(L, 1);

    // get request parameters
    lua_getfield(L, -1, ENDPOINT_PARMS);
    luaUserData_t* user_data = static_cast<luaUserData_t*>(lua_touserdata(L, -1));
    if(user_data && StringLib::match(RequestFields::OBJECT_TYPE, user_data->luaObj->getType()))
    {
        endpoint.request_parameters = dynamic_cast<RequestFields*>(user_data->luaObj);
    }
    else
    {
        endpoint.request_parameters = NULL;
    }
    lua_pop(L, 1);

    // return
    return endpoint;
}

/*----------------------------------------------------------------------------
 * logRequest
 *----------------------------------------------------------------------------*/
void LuaEndpoint::captureRequest (Request* request, const endpoint_t& endpoint, EventLib::tlm_input_t& tlm)
{
    mlog(endpoint.log_level, "%s %s: %s", verb2str(request->verb), request->resource, request->body);
    if(endpoint.request_parameters && endpoint.request_parameters->pointsInPolygon.value > 0)
    {
        const MathLib::coord_t& coord = endpoint.request_parameters->polygon[0];
        tlm.latitude = coord.lat;
        tlm.longitude = coord.lon;
    }
}

/*----------------------------------------------------------------------------
 * checkRole
 *----------------------------------------------------------------------------*/
void LuaEndpoint::checkRole (Request* request, const endpoint_t& endpoint)
{
    if(!endpoint.allowed_roles.empty())
    {
        /* Get and Check Roles from Request */
        const char* org_roles_hdr = request->getHdrOrgRoles();
        List<string*>* user_roles = StringLib::split(org_roles_hdr, StringLib::size(org_roles_hdr), ' ');
        bool match_found = false;
        for(const string& role: endpoint.allowed_roles)
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
            const FString error_msg("User must be a member to execute this endpoint");
            sendHeader(Unauthorized, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_UNAUTHORIZED, "%s", error_msg.c_str());
        }
    }
}

/*----------------------------------------------------------------------------
 * checkSignature
 *----------------------------------------------------------------------------*/
void LuaEndpoint::checkSignature (Request* request, const endpoint_t& endpoint)
{
    if(endpoint.signature_required)
    {
        const bool is_signed = request->verifyHdrSignature(request->getHdrAccount());
        if(!is_signed)
        {
            const FString error_msg("User must use a signed request for this endpoint");
            sendHeader(Unauthorized, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_UNAUTHORIZED, "%s", error_msg.c_str());

        }
    }
}

/*----------------------------------------------------------------------------
 * selectOutput
 *----------------------------------------------------------------------------*/
EndpointObject::content_t LuaEndpoint::selectOutput (Request* request, const endpoint_t& endpoint, const string& extension)
{
    /* Select Output */
    string* accept_hdr;
    if(request->headers.find("Accept", &accept_hdr)) // user requested output via header
    {
        const content_t accepted_content = str2content(accept_hdr->c_str());
        if(accepted_content == UNKNOWN)
        {
            const FString error_msg("Unsupported output in accept header: %s", accept_hdr->c_str());
            sendHeader(Not_Acceptable, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
        }
        else if(std::find(endpoint.supported_outputs.begin(), endpoint.supported_outputs.end(), accepted_content) == endpoint.supported_outputs.end())
        {
            const FString error_msg("Endpoint %s does not support %s output", request->resource, content2str(accepted_content));
            sendHeader(Not_Acceptable, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
        }
        return accepted_content; // use requested output
    }
    else if(!extension.empty()) // user requested output via extension
    {
        const content_t extension_content = str2content(extension.c_str());
        if(extension_content == UNKNOWN)
        {
            const FString error_msg("Unsupported output in extension: %s", extension.c_str());
            sendHeader(Not_Acceptable, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
        }
        else if(std::find(endpoint.supported_outputs.begin(), endpoint.supported_outputs.end(), extension_content) == endpoint.supported_outputs.end())
        {
            const FString error_msg("Endpoint %s does not support %s output", request->resource, content2str(extension_content));
            sendHeader(Not_Acceptable, content2str(TEXT), &request->rspq, error_msg.c_str());
            throw RunTimeException(CRITICAL, RTE_FAILURE, "%s", error_msg.c_str());
        }
        return extension_content; // use requested output

    }
    else if(!endpoint.supported_outputs.empty()) // nothing was requested
    {
        return endpoint.supported_outputs.front(); // use first output provided by endpoint
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
        const FString error_msg("Memory (%d%%) exceeded threshold, not performing request: %s", (int)(memory_usage * 100.0), request->resource);
        sendHeader(Service_Unavailable, content2str(TEXT), &request->rspq, error_msg.c_str());
        throw RunTimeException(ERROR, RTE_NOT_ENOUGH_MEMORY, "%s", error_msg.c_str());
    }
}

/*----------------------------------------------------------------------------
 * executeEndpoint
 *----------------------------------------------------------------------------*/
void LuaEndpoint::executeEndpoint (Request* request, LuaEngine* engine, content_t selected_output, const string& arguments)
{
    handler_f handler = retrieveHandler(selected_output); // returns NULL if no handler is found
    if(handler)
    {
        handler(request, engine, selected_output, arguments.c_str());
    }
    else
    {
        const FString error_msg("Unable to handle requested format: %s", content2str(selected_output));
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

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, request->trace_id, "lua_endpoint", "{\"verb\":\"%s\", \"resource\":\"%s\"}", verb2str(request->verb), request->resource);

    /* Initialize Telemetry */
    EventLib::tlm_input_t tlm = {
        .source_ip = request->getHdrSourceIp(),
        .endpoint = request->resource,
        .client = request->getHdrClient(),
        .account = request->getHdrAccount()
    };

    /* Initialize Lua Engine */
    LuaEngine engine(trace_id, NULL); // TODO: implement lua hook that checks for the timeout to have expired

    /* Get Script Parameters */
    const LuaEngine::script_t script = LuaEngine::sanitize(request->resource);

    /* Execute Lua Script */
    try
    {
        const endpoint_t endpoint = loadLuaScript(request, &engine, script.path); // throws on error
        captureRequest(request, endpoint, tlm); // logs request and populates additional telemetry
        checkRole(request, endpoint); // throws on error
        checkSignature(request, endpoint); // throws on error
        const content_t selected_output = selectOutput(request, endpoint, script.extension); // throws on error, returns output format
        checkMemoryUsage(request); // throws on error
        executeEndpoint(request, &engine, selected_output, script.argument); // executes registered handler, throws on error
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "%s", e.what());
        tlm.code = e.code();
    }

    /* End Response */
    const int rc = request->rspq.postCopy("", 0, SystemConfig::settings().publishTimeoutMs.value);
    if(rc <= 0) mlog(CRITICAL, "Failed to post terminator on %s: %d", request->rspq.getName(), rc);

    /* Generate Telemetry */
    tlm.duration = static_cast<float>(TimeLib::latchtime() - start);
    telemeter(INFO, tlm);

    /* Clean Up */
    delete request;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
