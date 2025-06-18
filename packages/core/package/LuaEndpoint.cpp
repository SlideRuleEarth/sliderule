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
 * handleRequest - returns true if streaming (chunked) response
 *----------------------------------------------------------------------------*/
bool LuaEndpoint::handleRequest (Request* request)
{
    // Allocate and Initialize Endpoint Info Struct
    info_t* info = new info_t;
    info->endpoint = this;
    info->request = request;
    info->streaming = true;

    // Determine Streaming
    if(request->verb == GET)
    {
        info->streaming = false;
    }
    else
    {
        // Check Header (needed for web javascript client, potentially others)
        // some clients do not allow a GET to have a request body
        // but SlideRule supports GETs on endpoints that use a
        // request body to determine what is sent in the response;
        // in those cases, the client can issue a POST with a request
        // body and provide this header set to "0", and SlideRule
        // will treat it like a GET and return a normal response
        // based on the contents of the request body.
        const char* streaming_header = request->getHdrStreaming();
        if(streaming_header != NULL && StringLib::match(streaming_header, "0"))
        {
            info->streaming = false;
        }
    }

    // Start Thread
    const Thread pid(requestThread, info, false);

    // Return Response Type
    return info->streaming;
}

/*----------------------------------------------------------------------------
 * requestThread
 *----------------------------------------------------------------------------*/
void* LuaEndpoint::requestThread (void* parm)
{
    info_t* info = static_cast<info_t*>(parm);
    EndpointObject::Request* request = info->request;
    LuaEndpoint* lua_endpoint = dynamic_cast<LuaEndpoint*>(info->endpoint);
    const double start = TimeLib::latchtime();
    int status_code = RTE_STATUS;

    /* Get Request Script */
    const char* script_pathname = LuaEngine::sanitize(request->resource);

    /* Start Trace */
    const uint32_t trace_id = start_trace(INFO, request->trace_id, "lua_endpoint", "{\"verb\":\"%s\", \"resource\":\"%s\"}", verb2str(request->verb), request->resource);

    /* Log Request */
    const event_level_t log_level =  info->streaming ? INFO : DEBUG;
    mlog(log_level, "%s %s: %s", verb2str(request->verb), request->resource, request->body);

    /* Create Publisher */
    Publisher* rspq = new Publisher(request->id);

    /* Check Authentication */
    const bool authorized = lua_endpoint->authenticate(request);

    /* Dispatch Handle Request */
    if(authorized)
    {
        /* Handle Response */
        if(info->streaming)
        {
            status_code = streamResponse(script_pathname, request, rspq, trace_id);
        }
        else
        {
            status_code = normalResponse(script_pathname, request, rspq, trace_id);
        }
    }
    else
    {
        /* Respond with Unauthorized Error */
        char header[MAX_HDR_SIZE];
        const int header_length = buildheader(header, Unauthorized);
        rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);
        status_code = RTE_UNAUTHORIZED;
    }

    /* End Response */
    const int rc = rspq->postCopy("", 0, SystemConfig::settings().publishTimeoutMs.value);
    if(rc <= 0)
    {
        mlog(CRITICAL, "Failed to post terminator on %s: %d", rspq->getName(), rc);
        status_code = RTE_DID_NOT_COMPLETE;
    }

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
    delete rspq;
    delete [] script_pathname;
    delete request;
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * normalResponse
 *----------------------------------------------------------------------------*/
int LuaEndpoint::normalResponse (const char* scriptpath, Request* request, Publisher* rspq, uint32_t trace_id)
{
    int status_code = RTE_STATUS;
    char header[MAX_HDR_SIZE];
    double mem;

    LuaEngine* engine = NULL;

    /* Check Memory */
    if( (SystemConfig::settings().normalMemoryThreshold.value >= 1.0) ||
        ((mem = OsApi::memusage()) < SystemConfig::settings().normalMemoryThreshold.value) )
    {
        /* Launch Engine */
        engine = new LuaEngine(scriptpath, reinterpret_cast<const char*>(request->body), trace_id, NULL, true);
        request->setLuaTable(engine->getLuaState(), request->id, "");
        const bool status = engine->executeEngine(SystemConfig::settings().requestTimeoutSec.value);

        /* Send Response */
        if(status)
        {
            const char* result = engine->getResult();
            if(result)
            {
                /* Success */
                const int result_length = StringLib::size(result);
                const int header_length = buildheader(header, OK, "text/plain", result_length, NULL, serverHead.c_str());
                rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);
                rspq->postCopy(result, result_length, SystemConfig::settings().publishTimeoutMs.value);
            }
            else
            {
                /* Missing Results */
                mlog(ERROR, "Script returned no results: %s", scriptpath);
                const char* error_msg = "Missing results";
                const int result_length = StringLib::size(error_msg);
                const int header_length = buildheader(header, Not_Found, "text/plain", result_length, NULL, serverHead.c_str());
                rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);
                rspq->postCopy(error_msg, result_length, SystemConfig::settings().publishTimeoutMs.value);
                status_code = RTE_SCRIPT_DOES_NOT_EXIST;
            }
        }
        else
        {
            /* Failed Execution */
            mlog(ERROR, "Failed to execute request: %s", scriptpath);
            const char* error_msg = "Failed execution";
            const int result_length = StringLib::size(error_msg);
            const int header_length = buildheader(header, Internal_Server_Error, "text/plain", result_length, NULL, serverHead.c_str());
            rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);
            rspq->postCopy(error_msg, result_length, SystemConfig::settings().publishTimeoutMs.value);
            status_code = RTE_FAILURE;
        }
    }
    else
    {
        /* Memory Exceeded */
        mlog(CRITICAL, "Memory (%d%%) exceeded threshold, not performing request: %s", (int)(mem * 100.0), scriptpath);
        const char* error_msg = "Memory exceeded";
        const int result_length = StringLib::size(error_msg);
        const int header_length = buildheader(header, Service_Unavailable, "text/plain", result_length, NULL, serverHead.c_str());
        rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);
        rspq->postCopy(error_msg, result_length, SystemConfig::settings().publishTimeoutMs.value);
        status_code = RTE_NOT_ENOUGH_MEMORY;
}

    /* Clean Up */
    delete engine;

    /* Return Status Code */
    return status_code;
}

/*----------------------------------------------------------------------------
 * streamResponse
 *----------------------------------------------------------------------------*/
int LuaEndpoint::streamResponse (const char* scriptpath, Request* request, Publisher* rspq, uint32_t trace_id)
{
    int status_code = RTE_STATUS;
    char header[MAX_HDR_SIZE];
    double mem;

    LuaEngine* engine = NULL;

    /* Check Memory */
    if( (SystemConfig::settings().streamMemoryThreshold.value >= 1.0) ||
        ((mem = OsApi::memusage()) < SystemConfig::settings().streamMemoryThreshold.value) )
    {
        /* Send Header */
        const int header_length = buildheader(header, OK, "application/octet-stream", 0, "chunked", serverHead.c_str());
        rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);

        /* Create Engine */
        engine = new LuaEngine(scriptpath, reinterpret_cast<const char*>(request->body), trace_id, NULL, true);

        /* Supply Global Variables to Script */
        request->setLuaTable(engine->getLuaState(), request->id, rspq->getName());

        /* Execute Engine
         *  The call to execute the script blocks on completion of the script. The lua state context
         *  is locked and cannot be accessed until the script completes */
        const bool status = engine->executeEngine(IO_PEND);

        /* Check Status
         *  At this point the http header has already been sent, so an error header cannot be sent;
         *  but we can log the error and report it as such in telemetry */
        if(!status)
        {
            mlog(CRITICAL, "Failed to execute script %s", scriptpath);
            status_code = RTE_FAILURE;
        }
    }
    else
    {
        mlog(CRITICAL, "Memory (%d%%) exceeded threshold, not performing request: %s", (int)(mem * 100.0), scriptpath);
        const int header_length = buildheader(header, Service_Unavailable);
        rspq->postCopy(header, header_length, SystemConfig::settings().publishTimeoutMs.value);
        status_code = RTE_NOT_ENOUGH_MEMORY;
    }

    /* Clean Up */
    delete engine;

    /* Return Status Code */
    return status_code;
}
