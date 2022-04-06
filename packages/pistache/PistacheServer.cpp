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

#include "PistacheServer.h"
#include "RouteHandler.h"
#include "core.h"

using namespace Pistache;
using namespace Rest;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* PistacheServer::LuaMetaName = "PistacheServer";
const struct luaL_Reg PistacheServer::LuaMetaTable[] = {
    {"route",       luaRoute},
    {NULL,          NULL}
};

StringLib::String PistacheServer::serverHead("sliderule/%s", LIBID);

const char* PistacheServer::RESPONSE_QUEUE = "rspq";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - endpoint(<port>, [<number of threads>])
 *----------------------------------------------------------------------------*/
int PistacheServer::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        long port_number = getLuaInteger(L, 1);
        long num_threads = getLuaInteger(L, 2, true, 1);

        /* Get Address */
        Port port(port_number);
        Address addr(Ipv4::any(), port);

        /* Create Lua Endpoint */
        return createLuaObject(L, new PistacheServer(L, addr, num_threads));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * str2verb
 *----------------------------------------------------------------------------*/
PistacheServer::verb_t PistacheServer::str2verb (const char* str)
{
         if(StringLib::match(str, "GET"))       return GET;
    else if(StringLib::match(str, "OPTIONS"))   return OPTIONS;
    else if(StringLib::match(str, "POST"))      return POST;
    else if(StringLib::match(str, "PUT"))       return PUT;
    else                                        return INVALID;
}

/*----------------------------------------------------------------------------
 * sanitize
 *
 *  Note: must delete returned string
 *----------------------------------------------------------------------------*/
const char* PistacheServer::sanitize (const char* filename)
{
    SafeString delimeter("%c", PATH_DELIMETER);
    SafeString safe_filename("%s", filename);
    safe_filename.replace(delimeter.getString(), "_");
    SafeString safe_pathname("%s%c%s%c%s.lua", CONFDIR, PATH_DELIMETER, "api", PATH_DELIMETER, safe_filename.getString());
    return safe_pathname.getString(true);
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
long PistacheServer::getUniqueId (char id_str[REQUEST_ID_LEN])
{
    if(numThreads > 1) idMut.lock();
    long id = requestId++;
    if(numThreads > 1) idMut.unlock();

    StringLib::format(id_str, REQUEST_ID_LEN, "%s.%ld", getName(), id);
    return id;
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PistacheServer::PistacheServer(lua_State* L,  Address addr, size_t num_threads):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    requestId(0),
    numThreads(num_threads),
    httpEndpoint(std::make_shared<Http::Endpoint>(addr))
{
    /* Set Number of Threads */
    auto opts = Http::Endpoint::options().threads(static_cast<int>(numThreads));
    httpEndpoint->init(opts);

    /* Set Default Routes */
    Routes::Post(router, "/echo", Routes::bind(&PistacheServer::echoHandler, this));
    Routes::Get(router, "/info", Routes::bind(&PistacheServer::infoHandler, this));
    Routes::Get(router, "/source/:name", Routes::bind(&PistacheServer::sourceHandler, this));
    Routes::Post(router, "/source/:name", Routes::bind(&PistacheServer::engineHandler, this));

    /* Create Server Thread */
    active = true;
    serverPid = new Thread(serverThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
PistacheServer::~PistacheServer(void)
{
    active = false;
    delete serverPid;

    mlog(CRITICAL, "Shutting down HTTP endpoints on port %s", httpEndpoint->getPort().toString().c_str());
    httpEndpoint->shutdown();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * echoHandler
 *----------------------------------------------------------------------------*/
void PistacheServer::echoHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Start Trace */
    uint32_t trace_id = start_trace(CRITICAL, traceId, "echo_handler", "{\"rqst_id\":\"%s\"}", id_str);

    /* Log Request */
    mlog(DEBUG, "request: %s at %s", id_str, request.resource().c_str());

    /* Build Header */
    response.headers().add<Http::Header::Server>(serverHead.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Text, Plain));

    /* Send Response */
    response.send(Http::Code::Ok, request.body().c_str());

    /* Stop Trace */
    stop_trace(CRITICAL, trace_id);
}

/*----------------------------------------------------------------------------
 * infoHandler
 *----------------------------------------------------------------------------*/
void PistacheServer::infoHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Start Trace */
    uint32_t trace_id = start_trace(CRITICAL, traceId, "info_handler", "{\"rqst_id\":\"%s\"}", id_str);

    /* Log Request */
    mlog(DEBUG, "request: %s at %s", id_str, request.resource().c_str());

    /* Build Header */
    response.headers().add<Http::Header::Server>(serverHead.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Text, Plain));

    /* Build Response */
    SafeString rsp("{\"apis\": [\"/echo\", \"/info\", \"/source/:name\", \"/engine/:name\"] }");

    /* Send Response */
    response.send(Http::Code::Ok, rsp.getString());

    /* Stop Trace */
    stop_trace(CRITICAL, trace_id);
}

/*----------------------------------------------------------------------------
 * sourceHandler
 *----------------------------------------------------------------------------*/
void PistacheServer::sourceHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Get Request Parmeters */
    std::string script_name = request.param(":name").as<std::string>();

    /* Start Trace */
    uint32_t trace_id = start_trace(CRITICAL, traceId, "source_handler", "{\"rqst_id\":\"%s\", \"script\":\"%s\"}", id_str, script_name.c_str());

    /* Log Request */
    mlog(DEBUG, "request: %s at %s", id_str, request.resource().c_str());

    /* Build Header */
    response.headers().add<Http::Header::Server>(serverHead.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Text, Plain));

    /* Launch Engine */
    const char* script_pathname = sanitize(script_name.c_str());
    LuaEngine* engine = new LuaEngine(script_pathname, request.body().c_str(), trace_id, NULL, true);
    bool status = engine->executeEngine(MAX_RESPONSE_TIME_MS);

    /* Send Response */
    if(status)
    {
        const char* result = engine->getResult();
        if(result)
        {
            response.send(Http::Code::Ok, result);
        }
        else
        {
            response.send(Http::Code::Not_Found, "Not Found");
        }
    }
    else
    {
        response.send(Http::Code::Request_Timeout, "Request Timeout");
    }

    /* Clean Up */
    delete engine;
    delete [] script_pathname;

    /* Stop Trace */
    stop_trace(CRITICAL, trace_id);
}

/*----------------------------------------------------------------------------
 * engineHandler
 *----------------------------------------------------------------------------*/
void PistacheServer::engineHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Get Request Parmeters */
    std::string script_name = request.param(":name").as<std::string>();

    /* Start Trace */
    uint32_t trace_id = start_trace(CRITICAL, traceId, "engine_handler", "{\"rqst_id\":\"%s\", \"script\":\"%s\"}", id_str, script_name.c_str());

    /* Log Request */
    mlog(DEBUG, "request: %s at %s", id_str, request.resource().c_str());

    /* Build Header */
    response.headers().add<Http::Header::Server>(serverHead.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Application, OctetStream));

    /* Create Engine */
    const char* script_pathname = sanitize(script_name.c_str());
    LuaEngine* engine = new LuaEngine(script_pathname, request.body().c_str(), trace_id, NULL, true);

    /* Supply and Setup Request Queue */
    engine->setString(RESPONSE_QUEUE, id_str);
    Subscriber rspq(id_str);

    /* Execute Engine
     *  the call to execute the script returns immediately (due to IO_CHECK) at which point
     *  the lua state context is locked and cannot be accessed until the script completes */
    engine->executeEngine(IO_CHECK);

    /* Stream Response
     *  the response is read from the response queue until both the script completes and
     *  there are no more messages left in the message queue */
    int status = MsgQ::STATE_OKAY;
    auto stream = response.stream(Http::Code::Ok);
    while(engine->isActive() || status == MsgQ::STATE_OKAY)
    {
        Subscriber::msgRef_t ref;
        status = rspq.receiveRef(ref, SYS_TIMEOUT);
        if(status == MsgQ::STATE_OKAY)
        {
            uint32_t size = ref.size;
            if(size > 0)
            {
                stream.write((const char*)&size, sizeof(size));
                stream.write((const char*)ref.data, ref.size);
            }
            else
            {
                stream.ends();
            }
            rspq.dereference(ref);
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            stream.flush();
        }
        else
        {
            mlog(CRITICAL, "%s error streaming data: %d", id_str, status);
            break;
        }
    }

    /* Clean Up */
    delete engine;
    delete [] script_pathname;

    /* Stop Trace */
    stop_trace(CRITICAL, trace_id);
}

/*----------------------------------------------------------------------------
 * serverThread
 *----------------------------------------------------------------------------*/
void* PistacheServer::serverThread (void* parm)
{
    PistacheServer* server = (PistacheServer*)parm;

    try
    {
        server->httpEndpoint->setHandler(server->router.handler());
        server->httpEndpoint->serveThreaded();
    }
    catch(const std::exception& e)
    {
        mlog(CRITICAL, "Failed to start server thread for %s: %s", server->getName(), e.what());
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaRoute - :route(<action>, <url>, <route handler>)
 *----------------------------------------------------------------------------*/
int PistacheServer::luaRoute(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        PistacheServer* lua_obj = (PistacheServer*)getLuaSelf(L, 1);

        /* Get Action */
        verb_t action = INVALID;
        if(lua_isnumber(L, 2))
        {
            action = (verb_t)getLuaInteger(L, 2);
        }
        else
        {
            const char* action_str = getLuaString(L, 2);
            action = str2verb(action_str);
        }

        /* Check Action */
        if(action != GET && action != POST && action != PUT)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid action: %d", action);
        }

        /* Get URL */
        const char* url = getLuaString(L, 3);

        /* Get Route Handler */
        RouteHandler* handler = (RouteHandler*)getLuaObject(L, 4, RouteHandler::OBJECT_TYPE);

        /* Set Route */
        if(action == GET)
        {
            Routes::Get(lua_obj->router, url, Routes::bind(handler->getHandler()));
            status = true;
        }
        else if(action == POST)
        {
            Routes::Post(lua_obj->router, url, Routes::bind(handler->getHandler()));
            status = true;
        }
        else if(action == PUT)
        {
            Routes::Put(lua_obj->router, url, Routes::bind(handler->getHandler()));
            status = true;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error binding route: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
