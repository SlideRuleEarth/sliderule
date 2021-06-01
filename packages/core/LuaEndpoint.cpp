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
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaEndpoint::LuaMetaName = "LuaEndpoint";
const struct luaL_Reg LuaEndpoint::LuaMetaTable[] = {
    {"metric",      luaMetric},
    {NULL,          NULL}
};

SafeString LuaEndpoint::ServerHeader("sliderule/%s", LIBID);

const char* LuaEndpoint::RESPONSE_QUEUE = "rspq";

const char* LuaEndpoint::CATCHALL_METRIC = "catchall";
const char* LuaEndpoint::HITS_METRIC = "hits";

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
        mlog(CRITICAL, "Error creating %s: %s", LuaMetaName, e.what());
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
    EndpointObject(L, LuaMetaName, LuaMetaTable),
    metricIds(INITIAL_NUM_ENDPOINTS)
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
    uint32_t trace_id = start_trace(INFO, lua_endpoint->getTraceId(), "lua_endpoint", "{\"rqst_id\":\"%s\", \"verb\":\"%s\", \"url\":\"%s\"}", request->id, verb2str(request->verb), request->url);

    /* Log Request */
    mlog(INFO, "%s request at %s to %s", verb2str(request->verb), request->id, script_pathname);

    /* Update Metrics */
    int32_t metric_id = lua_endpoint->getMetricId(request->url);
    if(metric_id != EventLib::INVALID_METRIC)
    {
        EventLib::incrementMetric(metric_id);
    }

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
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}

/*----------------------------------------------------------------------------
 * handleRequest
 *----------------------------------------------------------------------------*/
EndpointObject::rsptype_t LuaEndpoint::handleRequest (request_t* request)
{
    /* Start Thread */
    request->pid = new Thread(requestThread, request); // detached

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
    LuaEngine* engine = new LuaEngine(scriptpath, body, trace_id, NULL, true);
    bool status = engine->executeEngine(MAX_RESPONSE_TIME_MS);

    /* Send Response */
    if(status)
    {
        const char* result = engine->getResult();
        if(result)
        {
            int result_length = StringLib::size(result, MAX_SOURCED_RESPONSE_SIZE);
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
    LuaEngine* engine = new LuaEngine(scriptpath, body, trace_id, NULL, true);

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
 * getMetricId
 *----------------------------------------------------------------------------*/
int32_t LuaEndpoint::getMetricId (const char* endpoint)
{
    int32_t metric_id = EventLib::INVALID_METRIC;

    try
    {
        metric_id = metricIds[endpoint];
    }
    catch (const RunTimeException& e1)
    {
        (void)e1;

        try
        {
            metric_id = metricIds[CATCHALL_METRIC];            
        }
        catch(const RunTimeException& e2)
        {
            (void)e2;
        }
    }

    return metric_id;
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
    SafeString safe_pathname("%s%c%s.lua", CONFDIR, PATH_DELIMETER, safe_filename.getString());
    return safe_pathname.getString(true);
}

/*----------------------------------------------------------------------------
 * luaMetric - :metric(<endpoint name>)
 * 
 * Note: NOT thread safe, must be called prior to attaching endpoint to server
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaMetric (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LuaEndpoint* lua_obj = (LuaEndpoint*)getLuaSelf(L, 1);

        /* Get Endpoint Name */
        const char* endpoint_name = getLuaString(L, 2, true, CATCHALL_METRIC);

        /* Get Object Name */
        const char* obj_name = lua_obj->getName();

        /* Register Metrics */
        int32_t id = EventLib::registerMetric(obj_name, "%s.%s", endpoint_name, HITS_METRIC);
        if(id == EventLib::INVALID_METRIC)
        {
            throw RunTimeException("Registry failed for %s.%s", obj_name, endpoint_name);
        }

        /* Add to Metric Ids */
        if(!lua_obj->metricIds.add(endpoint_name, id, true))
        {
            throw RunTimeException("Could not associate metric id to endpoint");
        }

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating metric: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
