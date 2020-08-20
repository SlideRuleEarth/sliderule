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

#include "LuaEndpoint.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaEndpoint::LuaMetaName = "LuaEndpoint";
const struct luaL_Reg LuaEndpoint::LuaMetaTable[] = {
    {NULL,          NULL}
};

SafeString LuaEndpoint::ServerHeader("sliderule/%s", BINID);

const char* LuaEndpoint::RESPONSE_QUEUE = "rspq";

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
LuaEndpoint::LuaEndpoint(lua_State* L):
    EndpointObject(L, LuaMetaName, LuaMetaTable)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::~LuaEndpoint(void)
{
}

/*----------------------------------------------------------------------------
 * requestThread
 *----------------------------------------------------------------------------*/
void* LuaEndpoint::requestThread (void* parm)
{
    request_t* request = (request_t*)parm;
    LuaEndpoint* lua_endpoint = (LuaEndpoint*)request->endpoint;

    /* Get Request Script */
    const char* script_pathname = sanitize(request->url);

    /* Start Trace */
    uint32_t trace_id = start_trace_ext(lua_endpoint->getTraceId(), "lua_endpoint", "{\"rqst_id\":\"%s\", \"verb\":\"%s\", \"url\":\"%s\"}", request->id, verb2str(request->verb), request->url);

    /* Log Request */
    mlog(INFO, "%s request at %s to %s\n", verb2str(request->verb), request->id, script_pathname);

    /* Create Publisher */
    Publisher* rspq = new Publisher(request->id);

    /* Dispatch Handle Request */
    switch(request->verb)
    {
        case GET:   lua_endpoint->returnResponse(script_pathname, request->body, rspq, trace_id); break;
        case POST:  lua_endpoint->streamResponse(script_pathname, request->body, rspq, trace_id); break;
        default:    break;
    }

    /* Clean Up */
    delete rspq;
    delete [] script_pathname;

    /* Stop Trace */
    stop_trace(trace_id);

    /* Remove from Pid Table */
    lua_endpoint->pidMut.lock();
    {
        lua_endpoint->pidTable.remove(request->id);
    }
    lua_endpoint->pidMut.unlock();

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * handleRequest
 *----------------------------------------------------------------------------*/
EndpointObject::rsptype_t LuaEndpoint::handleRequest (request_t* request)
{
    LuaEndpoint* lua_endpoint = (LuaEndpoint*)request->endpoint;

    /* Start Thread */
    Thread* pid = new Thread(requestThread, request, false); // detached
    delete pid; // once thread is kicked off and detached, it is safe to delete

    /* Add PID to Table */
    lua_endpoint->pidMut.lock();
    {
        lua_endpoint->pidTable.add(request->id, pid);
    }
    lua_endpoint->pidMut.unlock();

    /* Return Response Type */
    if(request->verb == POST)   return STREAMING;
    else                        return NORMAL;
}

/*----------------------------------------------------------------------------
 * returnResponse
 *----------------------------------------------------------------------------*/
void LuaEndpoint::returnResponse (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id)
{
    char header[MAX_HDR_SIZE];

    /* Launch Engine */
    LuaEngine* engine = new LuaEngine(rspq->getName(), scriptpath, body, trace_id, NULL, true);
    bool status = engine->executeEngine(MAX_RESPONSE_TIME_MS);

    /* Send Response */
    if(status)
    {
        const char* result = engine->getResult();
        if(result)
        {
            int result_length = StringLib::size(result);
            int header_length = buildheader(header, OK, "text/plain", result_length, NULL, ServerHeader.getString());
            rspq->postCopy(header, header_length);
            rspq->postCopy(result, result_length);
        }
        else
        {
            int header_length = buildheader(header, Not_Found);
            rspq->postCopy(header, header_length);
        }
    }
    else
    {
        int header_length = buildheader(header, Request_Timeout);
        rspq->postCopy(header, header_length);
    }

    /* End Response */
    rspq->postCopy("", 0);

    /* Clean Up */
    delete engine;
}

/*----------------------------------------------------------------------------
 * streamResponse
 *----------------------------------------------------------------------------*/
void LuaEndpoint::streamResponse (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id)
{
    char header[MAX_HDR_SIZE];

    /* Send Header */
    int header_length = buildheader(header, OK, "application/octet-stream", 0, "chunked", ServerHeader.getString());
    rspq->postCopy(header, header_length);

    /* Create Engine */
    LuaEngine* engine = new LuaEngine(rspq->getName(), scriptpath, body, trace_id, NULL, true);

    /* Supply and Setup Request Queue */
    engine->setString(RESPONSE_QUEUE, rspq->getName());

    /* Execute Engine
     *  The call to execute the script blocks on completion of the script. The lua state context 
     *  is locked and cannot be accessed until the script completes */
    engine->executeEngine(IO_PEND);

    /* End Response */
    rspq->postCopy("", 0);

    /* Clean Up */
    delete engine;
}

/*----------------------------------------------------------------------------
 * sanitize
 *
 *  Note: must delete returned string
 *----------------------------------------------------------------------------*/
const char* LuaEndpoint::sanitize (const char* filename)
{
    SafeString delimeter("%c", PATH_DELIMETER);
    SafeString safe_filename("%s", filename);
    safe_filename.replace(delimeter.getString(), "_");
    SafeString safe_pathname("%s%c%s.lua", CONFIGPATH, PATH_DELIMETER, safe_filename.getString());
    return safe_pathname.getString(true);
}
