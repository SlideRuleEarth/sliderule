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
 * handleRequest
 *----------------------------------------------------------------------------*/
LuaEndpoint::code_t LuaEndpoint::handleRequest (const char* id ,const char* url, verb_t verb, Dictionary<const char*>& headers, const char* body, EndpointObject* self)
{
    (void)headers;

    LuaEndpoint* endpoint = (LuaEndpoint*)self;

    /* Get Request Script */
    const char* script_pathname = sanitize(url);

    /* Start Trace */
    uint32_t trace_id = start_trace_ext(endpoint->traceId, "lua_endpoint", "{\"rqst_id\":\"%s\", \"verb\":\"%s\", \"url\":\"%s\"}", id, verb2str(verb), url);

    /* Log Request */
    mlog(INFO, "%s request at %s with %s\n", verb2str(verb), id, script_pathname);

    /* Create Publisher */
    Publisher* rspq = new Publisher(id);

    /* Dispatch Handle Request */
    code_t status;
    switch(verb)
    {
        case GET:   status = endpoint->returnResponse(script_pathname, body, rspq, trace_id); break;
        case POST:  status = endpoint->streamResponse(script_pathname, body, rspq, trace_id); break;
        default:    status = Not_Found; break;
    }

    /* Clean Up */
    delete rspq;
    delete script_pathname;

    /* Stop Trace */
    stop_trace(trace_id);

    /* Return Status Code */
    return status;
}


/*----------------------------------------------------------------------------
 * returnResponse
 *----------------------------------------------------------------------------*/
EndpointObject::code_t LuaEndpoint::returnResponse (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id)
{
    char header[MAX_HDR_SIZE];
    code_t status_code;

    /* Launch Engine */
    LuaEngine* engine = new LuaEngine(rspq->getName(), scriptpath, body, trace_id, NULL, true);
    bool status = engine->executeEngine(MAX_RESPONSE_TIME_MS);

    /* Send Response */
    if(status)
    {
        const char* result = engine->getResult();
        if(result)
        {
            status_code = OK;
            int result_length = StringLib::size(result);
            int header_length = buildheader(header, status_code, "text/plain", result_length, NULL, ServerHeader.getString());
            rspq->postCopy(header, header_length);
            rspq->postCopy(result, result_length);
        }
        else
        {
            status_code = Not_Found;
            int header_length = buildheader(header, status_code);
            rspq->postCopy(header, header_length);
        }
    }
    else
    {
        status_code = Request_Timeout;
        int header_length = buildheader(header, status_code);
        rspq->postCopy(header, header_length);
    }

    /* End Response */
    rspq->postCopy("", 0);
    
    /* Clean Up */
    delete engine;

    /* Return Status Code */
    return status_code;
}

/*----------------------------------------------------------------------------
 * streamResponse
 *----------------------------------------------------------------------------*/
EndpointObject::code_t LuaEndpoint::streamResponse (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id)
{
    char header[MAX_HDR_SIZE];
    code_t status_code;

    /* Send Header */
    status_code = OK;
    int header_length = buildheader(header, status_code, "application/octet-stream", 0, "chunked", ServerHeader.getString());
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
    // HOW TO KNOW WHEN TO END...
//    rspq->postCopy(RESPONSE_END, RESPONSE_END_SIZE);

    /* Clean Up */
    delete engine;

    /* Return Status Code */
    return status_code;
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
