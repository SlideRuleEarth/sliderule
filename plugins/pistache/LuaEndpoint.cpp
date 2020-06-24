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
#include "RouteHandler.h"
#include "core.h"

using namespace Pistache;
using namespace Rest;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaEndpoint::LuaMetaName = "LuaEndpoint";
const struct luaL_Reg LuaEndpoint::LuaMetaTable[] = {
    {"route",       luaRoute},
    {NULL,          NULL}
};

StringLib::String LuaEndpoint::ServerHeader("sliderule/%s", BINID);

const char* LuaEndpoint::RESPONSE_QUEUE = "rspq";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - endpoint(<port>, [<number of threads>])
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaCreate (lua_State* L)
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
        return createLuaObject(L, new LuaEndpoint(L, addr, num_threads));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * str2verb
 *----------------------------------------------------------------------------*/
LuaEndpoint::verb_t LuaEndpoint::str2verb (const char* str)
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
const char* LuaEndpoint::sanitize (const char* filename)
{
    SafeString delimeter("%c", PATH_DELIMETER);
    SafeString safe_filename("%s", filename);
    safe_filename.replace(delimeter.getString(), "_");
    SafeString safe_pathname("%s%c%s.lua", CONFIGPATH, PATH_DELIMETER, safe_filename.getString());
    return safe_pathname.getString(true);
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
long LuaEndpoint::getUniqueId (char id_str[REQUEST_ID_LEN])
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
LuaEndpoint::LuaEndpoint(lua_State* L,  Address addr, size_t num_threads):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    requestId(0),
    numThreads(num_threads),
    httpEndpoint(std::make_shared<Http::Endpoint>(addr))
{
    /* Set Number of Threads */
    auto opts = Http::Endpoint::options().threads(static_cast<int>(num_threads));
    httpEndpoint->init(opts);

    /* Set Default Routes */
    Routes::Post(router, "/echo", Routes::bind(&LuaEndpoint::echoHandler, this));
    Routes::Get(router, "/info", Routes::bind(&LuaEndpoint::infoHandler, this));
    Routes::Post(router, "/source/:name", Routes::bind(&LuaEndpoint::sourceHandler, this));
    Routes::Post(router, "/engine/:name", Routes::bind(&LuaEndpoint::engineHandler, this));

    /* Create Server Thread */
    active = true;
    serverPid = new Thread(serverThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaEndpoint::~LuaEndpoint(void)
{
    active = false;
    delete serverPid;

    mlog(CRITICAL, "Shutting down HTTP endpoints on port %s\n", httpEndpoint->getPort().toString().c_str());
    httpEndpoint->shutdown();
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * echoHandler
 *----------------------------------------------------------------------------*/
void LuaEndpoint::echoHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Log Request */
    mlog(INFO, "request: %s at %s\n", id_str, request.resource().c_str());

    /* Build Header */
    response.headers().add<Http::Header::Server>(ServerHeader.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Text, Plain));

    /* Send Response */
    response.send(Http::Code::Ok, request.body().c_str());
}

/*----------------------------------------------------------------------------
 * infoHandler
 *----------------------------------------------------------------------------*/
void LuaEndpoint::infoHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Log Request */
    mlog(INFO, "request: %s at %s\n", id_str, request.resource().c_str());

    /* Build Header */
    response.headers().add<Http::Header::Server>(ServerHeader.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Text, Plain));

    /* Build Response */
    SafeString rsp("{\"apis\": [\"/echo\", \"/info\", \"/source/:name\", \"/engine/:name\"] }");

    /* Send Response */
    response.send(Http::Code::Ok, rsp.getString());
}

/*----------------------------------------------------------------------------
 * sourceHandler
 *----------------------------------------------------------------------------*/
void LuaEndpoint::sourceHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Log Request */
    mlog(INFO, "request: %s at %s\n", id_str, request.resource().c_str());

    /* Get Request Parmeters */
    std::string script_name = request.param(":name").as<std::string>();

    /* Build Header */
    response.headers().add<Http::Header::Server>(ServerHeader.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Text, Plain));

    /* Launch Engine */
    const char* script_pathname = sanitize(script_name.c_str());
    LuaEngine* engine = new LuaEngine(id_str, script_pathname, request.body().c_str(), NULL, true);
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
}

/*----------------------------------------------------------------------------
 * engineHandler
 *----------------------------------------------------------------------------*/
void LuaEndpoint::engineHandler (const Rest::Request& request, Http::ResponseWriter response)
{
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str);

    /* Log Request */
    mlog(INFO, "request: %s at %s\n", id_str, request.resource().c_str());

    /* Get Request Parmeters */
    std::string script_name = request.param(":name").as<std::string>();

    /* Build Header */
    response.headers().add<Http::Header::Server>(ServerHeader.getString());
    response.headers().add<Http::Header::ContentType>(MIME(Application, OctetStream));

    /* Create Engine */
    const char* script_pathname = sanitize(script_name.c_str());
    LuaEngine* engine = new LuaEngine(id_str, script_pathname, request.body().c_str(), NULL, true);

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
            printf("SIZE: %d\n", size);
            stream.write((const char*)&size, sizeof(size));
            stream.write((const char*)ref.data, ref.size);
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            stream.flush();
        }
        else
        {
            mlog(CRITICAL, "rrror: %s streaming data: %d\n", id_str, status);
            break;
        }
    }
    stream.ends();

    /* Clean Up */
    delete engine;
    delete [] script_pathname;
}

/*----------------------------------------------------------------------------
 * serverThread
 *----------------------------------------------------------------------------*/
void* LuaEndpoint::serverThread (void* parm)
{
    LuaEndpoint* server = (LuaEndpoint*)parm;

    try
    {
        server->httpEndpoint->setHandler(server->router.handler());
        server->httpEndpoint->serveThreaded();
    }
    catch(const std::exception& e)
    {
        mlog(CRITICAL, "Failed to start server thread for %s: %s\n", server->getName(), e.what());
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaRoute - :route(<action>, <url>, <route handler>)
 *----------------------------------------------------------------------------*/
int LuaEndpoint::luaRoute(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        LuaEndpoint* lua_obj = (LuaEndpoint*)getLuaSelf(L, 1);

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
            throw LuaException("Invalid action: %d", action);
        }

        /* Get URL */
        const char* url = getLuaString(L, 3);

        /* Get Route Handler */
        RouteHandler* handler = (RouteHandler*)lockLuaObject(L, 4, RouteHandler::OBJECT_TYPE);

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
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error binding route: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
