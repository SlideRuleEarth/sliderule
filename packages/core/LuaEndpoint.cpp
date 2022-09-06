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
    {"auth",        luaAuth},
    {NULL,          NULL}
};

const char* LuaEndpoint::EndpointExceptionRecType = "exceptrec";
const RecordObject::fieldDef_t LuaEndpoint::EndpointExceptionRecDef[] = {
    {"code",        RecordObject::INT32,    offsetof(response_exception_t, code), 1,                        NULL, NATIVE_FLAGS},
    {"text",        RecordObject::STRING,   offsetof(response_exception_t, text), MAX_EXCEPTION_TEXT_SIZE,  NULL, NATIVE_FLAGS}
};

const double LuaEndpoint::DEFAULT_NORMAL_REQUEST_MEMORY_THRESHOLD = 1.0;
const double LuaEndpoint::DEFAULT_STREAM_REQUEST_MEMORY_THRESHOLD = 1.0;

SafeString LuaEndpoint::serverHead("sliderule/%s", LIBID);

const char* LuaEndpoint::LUA_ALIVE_FUNC = "__alive";
const char* LuaEndpoint::LUA_RESPONSE_QUEUE = "rspq";
const char* LuaEndpoint::LUA_REQUEST_SELF = "__endpoint";

const char* LuaEndpoint::UNREGISTERED_ENDPOINT = "untracked";
const char* LuaEndpoint::HITS_METRIC = "hits";

int32_t LuaEndpoint::totalMetricId = EventLib::INVALID_METRIC;

/******************************************************************************
 * AUTHENTICATOR SUBCLASS
 ******************************************************************************/

const char* LuaEndpoint::Authenticator::OBJECT_TYPE = "Authenticator";
const char* LuaEndpoint::Authenticator::LuaMetaName = "Authenticator";
const struct luaL_Reg LuaEndpoint::Authenticator::LuaMetaTable[] = {
    {NULL,          NULL}
};

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::Authenticator::Authenticator(lua_State* L):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::Authenticator::~Authenticator(void)
{
}

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
bool LuaEndpoint::init (void)
{
    bool status = true;

    /* Register Metric */
    totalMetricId = EventLib::registerMetric(LuaEndpoint::LuaMetaName, EventLib::COUNTER, "%s.%s", UNREGISTERED_ENDPOINT, HITS_METRIC);
    if(totalMetricId == EventLib::INVALID_METRIC)
    {
        mlog(ERROR, "Registry failed for %s.%s", UNREGISTERED_ENDPOINT, HITS_METRIC);
        status = false;
    }

    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(EndpointExceptionRecType, "code", sizeof(response_exception_t), EndpointExceptionRecDef, sizeof(EndpointExceptionRecDef) / sizeof(RecordObject::fieldDef_t));
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d", EndpointExceptionRecType, rc);
        status = false;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * luaCreate - endpoint([<normal memory threshold>], [<stream memory threshold>])
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        double normal_mem_thresh = getLuaFloat(L, 1, true, DEFAULT_NORMAL_REQUEST_MEMORY_THRESHOLD);
        double stream_mem_thresh = getLuaFloat(L, 2, true, DEFAULT_STREAM_REQUEST_MEMORY_THRESHOLD);
        event_level_t lvl = (event_level_t)getLuaInteger(L, 3, true, INFO);

        /* Create Lua Endpoint */
        return createLuaObject(L, new LuaEndpoint(L, normal_mem_thresh, stream_mem_thresh, lvl));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * generateExceptionStatus
 *----------------------------------------------------------------------------*/
void LuaEndpoint::generateExceptionStatus (int code, Publisher* outq, bool* active, const char* errmsg, ...)
{
    /* Build Error Message */
    char error_buf[MAX_EXCEPTION_TEXT_SIZE];
    va_list args;
    va_start(args, errmsg);
    int vlen = vsnprintf(error_buf, MAX_EXCEPTION_TEXT_SIZE - 1, errmsg, args);
    int attr_size = MAX(MIN(vlen + 1, MAX_EXCEPTION_TEXT_SIZE), 1);
    error_buf[attr_size - 1] = '\0';
    va_end(args);

    /* Initialize Endpoint Exception Record */
    RecordObject record(EndpointExceptionRecType);
    response_exception_t* exception = (response_exception_t*)record.getRecordData();
    exception->code = code;
    StringLib::format(exception->text, MAX_EXCEPTION_TEXT_SIZE, "%s", error_buf);

    /* Post Exception Record */
    uint8_t* rec_buf = NULL;
    int rec_bytes = record.serialize(&rec_buf, RecordObject::REFERENCE);
    int post_status = MsgQ::STATE_TIMEOUT;
    if(active) while((*active) && (post_status = outq->postCopy(rec_buf, rec_bytes, SYS_TIMEOUT)) == MsgQ::STATE_TIMEOUT);
    else outq->postCopy(rec_buf, rec_bytes, IO_CHECK);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::LuaEndpoint(lua_State* L, double normal_mem_thresh, double stream_mem_thresh, event_level_t lvl):
    EndpointObject(L, LuaMetaName, LuaMetaTable),
    metricIds(INITIAL_NUM_ENDPOINTS),
    normalRequestMemoryThreshold(normal_mem_thresh),
    streamRequestMemoryThreshold(stream_mem_thresh),
    logLevel(lvl),
    authenticator(NULL)
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
    const char* script_pathname = LuaEngine::sanitize(request->url);

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, lua_endpoint->getTraceId(), "lua_endpoint", "{\"rqst_id\":\"%s\", \"verb\":\"%s\", \"url\":\"%s\"}", request->id, verb2str(request->verb), request->url);

    /* Log Request */
    mlog(lua_endpoint->logLevel, "%s %s: %s", verb2str(request->verb), request->url, request->body);

    /* Update Metrics */
    int32_t metric_id = lua_endpoint->getMetricId(request->url);
    if(metric_id != EventLib::INVALID_METRIC)
    {
        increment_metric(DEBUG, metric_id);
    }

    /* Create Publisher */
    Publisher* rspq = new Publisher(request->id);

    /* Check Authentication */
    bool authorized = false;
    if(lua_endpoint->authenticator)
    {
        const char* auth_value = NULL;
        if(request->headers->find("Authorization", &auth_value))
        {
            const char* token = StringLib::find(auth_value, ' ');
            if(token)
            {
                authorized = lua_endpoint->authenticator->isValid(token + 1);
            }
        }
    }
    else // no authentication required
    {
        authorized = true;
    }

    /* Dispatch Handle Request */
    if(authorized)
    {
        switch(request->verb)
        {
            case GET:   lua_endpoint->normalResponse(script_pathname, request, rspq, trace_id); break;
            case POST:  lua_endpoint->streamResponse(script_pathname, request, rspq, trace_id); break;
            default:    break;
        }
    }
    else
    {
        char header[MAX_HDR_SIZE];
        int header_length = buildheader(header, Unauthorized);
        rspq->postCopy(header, header_length);
    }

    /* End Response */
    rspq->postCopy("", 0);

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
 * normalResponse
 *----------------------------------------------------------------------------*/
void LuaEndpoint::normalResponse (const char* scriptpath, request_t* request, Publisher* rspq, uint32_t trace_id)
{
    char header[MAX_HDR_SIZE];
    double mem;

    LuaEngine* engine = NULL;

    /* Check Memory */
    if( (normalRequestMemoryThreshold >= 1.0) ||
        ((mem = LocalLib::memusage()) < normalRequestMemoryThreshold) )
    {
        /* Launch Engine */
        engine = new LuaEngine(scriptpath, request->body, trace_id, NULL, true);
        bool status = engine->executeEngine(MAX_RESPONSE_TIME_MS);

        /* Send Response */
        if(status)
        {
            const char* result = engine->getResult();
            if(result)
            {
                int result_length = StringLib::size(result, MAX_SOURCED_RESPONSE_SIZE);
                int header_length = buildheader(header, OK, "text/plain", result_length, NULL, serverHead.getString());
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
            mlog(ERROR, "Failed to execute request: %s", scriptpath);
            int header_length = buildheader(header, Internal_Server_Error);
            rspq->postCopy(header, header_length);
        }
    }
    else
    {
        mlog(CRITICAL, "Memory (%d%%) exceeded threshold, not performing request: %s", (int)(mem * 100.0), scriptpath);
        int header_length = buildheader(header, Service_Unavailable);
        rspq->postCopy(header, header_length);
    }

    /* Clean Up */
    if(engine) delete engine;
}

/*----------------------------------------------------------------------------
 * streamResponse
 *----------------------------------------------------------------------------*/
void LuaEndpoint::streamResponse (const char* scriptpath, request_t* request, Publisher* rspq, uint32_t trace_id)
{
    char header[MAX_HDR_SIZE];
    double mem;

    LuaEngine* engine = NULL;

    /* Check Memory */
    if( (streamRequestMemoryThreshold >= 1.0) ||
        ((mem = LocalLib::memusage()) < streamRequestMemoryThreshold) )
    {
        /* Send Header */
        int header_length = buildheader(header, OK, "application/octet-stream", 0, "chunked", serverHead.getString());
        rspq->postCopy(header, header_length);

        /* Create Engine */
        engine = new LuaEngine(scriptpath, request->body, trace_id, NULL, true);

        /* Supply Global Variables to Script */
        engine->setString(LUA_RESPONSE_QUEUE, rspq->getName());
        engine->setObject(LUA_REQUEST_SELF, request);
        engine->setFunction(LUA_ALIVE_FUNC, luaAlive);

        /* Execute Engine
        *  The call to execute the script blocks on completion of the script. The lua state context
        *  is locked and cannot be accessed until the script completes */
        engine->executeEngine(IO_PEND);
    }
    else
    {
        mlog(CRITICAL, "Memory (%d%%) exceeded threshold, not performing request: %s", (int)(mem * 100.0), scriptpath);
        int header_length = buildheader(header, Service_Unavailable);
        rspq->postCopy(header, header_length);
    }

    /* Clean Up */
    if(engine) delete engine;
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
            metric_id = metricIds[UNREGISTERED_ENDPOINT];
        }
        catch(const RunTimeException& e2)
        {
            (void)e2;
        }
    }

    return metric_id;
}

/*----------------------------------------------------------------------------
 * luaAlive - __alive()
 *
 * Note:    this function is available only to the endpoint lua script, NOT the
 *          script that creates a LuaEndpoint object
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaAlive (lua_State* L)
{
    bool status;

    /* Retrieve LuaEngine from Registry */
    lua_pushstring(L, LuaEngine::LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX);
    LuaEngine* engine = (LuaEngine*)lua_touserdata(L, -1);
    if(engine)
    {
        /* Get LuaEngine State */
        status = engine->isActive();
        if(status)
        {
            /* Retrieve LuaEndpoint */
            lua_getglobal(L, LUA_REQUEST_SELF);
            request_t* request = (request_t*)lua_touserdata(L, -1);
            if(request)
            {
                status = request->active;
            }
            else
            {
                mlog(CRITICAL, "Unable to access request");
                status = false;
            }
        }
    }
    else
    {
        mlog(CRITICAL, "Unable to access lua engine");
        status = false;
    }

    /* Return Status */
    lua_pushboolean(L, status);
    return 1;
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
        const char* endpoint_name = getLuaString(L, 2);

        /* Get Object Name */
        const char* obj_name = lua_obj->getName();

        /* Register Metrics */
        int32_t id = EventLib::registerMetric(obj_name, EventLib::COUNTER, "%s.%s", endpoint_name, HITS_METRIC);
        if(id == EventLib::INVALID_METRIC)
        {
            throw RunTimeException(ERROR, RTE_ERROR, "Registry failed for %s.%s", obj_name, endpoint_name);
        }

        /* Add to Metric Ids */
        if(!lua_obj->metricIds.add(endpoint_name, id, true))
        {
            throw RunTimeException(ERROR, RTE_ERROR, "Could not associate metric id to endpoint");
        }

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating metric: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaAuth - :auth(<authentication object>)
 *
 * Note: NOT thread safe, must be called prior to attaching endpoint to server
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaAuth (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LuaEndpoint* lua_obj = (LuaEndpoint*)getLuaSelf(L, 1);

        /* Get Authenticator */
        Authenticator* auth = (Authenticator*)getLuaObject(L, 2, LuaEndpoint::Authenticator::OBJECT_TYPE);

        /* Set Authenticator */
        lua_obj->authenticator = auth;

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting authenticator: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
