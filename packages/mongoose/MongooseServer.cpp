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

#include <cesanta/mongoose.h>

#include "MongooseServer.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MongooseServer::LuaMetaName = "MongooseServer";
const struct luaL_Reg MongooseServer::LuaMetaTable[] = {
    {NULL,          NULL}
};

const char* MongooseServer::RESPONSE_QUEUE = "rspq";

std::atomic<uint32_t> MongooseServer::requestId{0};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - endpoint(<port>, [<number of threads>])
 *----------------------------------------------------------------------------*/
int MongooseServer::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* port_str = getLuaString(L, 1);
        long num_threads = getLuaInteger(L, 2, true, 1);

        /* Create Lua Endpoint */
        return createLuaObject(L, new MongooseServer(L, port_str, num_threads));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * str2verb
 *----------------------------------------------------------------------------*/
MongooseServer::verb_t MongooseServer::str2verb (const char* str)
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
const char* MongooseServer::sanitize (const char* filename)
{
    SafeString delimeter("%c", PATH_DELIMETER);
    SafeString safe_filename("%s", filename);
    safe_filename.replace(delimeter.getString(), "_");
    SafeString safe_pathname("%s%c%s.lua", CONFIGPATH, PATH_DELIMETER, safe_filename.getString());
    return safe_pathname.getString(true);
}

/*----------------------------------------------------------------------------
 * getEndpoint
 *----------------------------------------------------------------------------*/
const char* MongooseServer::getEndpoint (const char* url)
{
    const char* first_slash = StringLib::find(url, '/');
    if(first_slash)
    {
        const char* second_slash = StringLib::find(first_slash+1, '/');
        if(second_slash)
        {
            const char* first_space = StringLib::find(second_slash+1, ' ');
            if(first_space)
            {
                int endpoint_len = first_space - second_slash; // this include null terminator
                char* endpoint = new char[endpoint_len];
                const char* src = second_slash + 1;
                char* dst = endpoint;
                while(src < first_space)
                {
                    *dst++ = *src++;
                }
                *dst = '\0';
                return endpoint;                
            }
        }
    }
    
    mlog(CRITICAL, "Failed to parse url: %s\n", url);
    return NULL;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
long MongooseServer::getUniqueId (char id_str[REQUEST_ID_LEN], const char* name)
{
    long id = requestId++;
    StringLib::format(id_str, REQUEST_ID_LEN, "%s.%ld", name, id);
    return id;
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MongooseServer::MongooseServer(lua_State* L,  const char* _port, size_t num_threads):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    port = StringLib::duplicate(_port);
    numThreads = num_threads;
    active = true;
    serverPid = new Thread(serverThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
MongooseServer::~MongooseServer(void)
{
    mlog(CRITICAL, "Shutting down HTTP endpoints on port %s\n", port);
    active = false;
    delete serverPid;
    delete [] port;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * sourceHandler
 *----------------------------------------------------------------------------*/
void MongooseServer::sourceHandler (struct mg_connection *nc, struct http_message *hm)
{
    MongooseServer* server = (MongooseServer*)nc->mgr->user_data;
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str, server->getName());

    /* Get Request Parmeters */
    const char* script_name = getEndpoint(hm->uri.p);
    if(!script_name) return;

    /* Start Trace */
    uint32_t parent_id = server->traceId;
    uint32_t trace_id = start_trace_ext(parent_id, "source_handler", "{\"rqst_id\":\"%s\", \"script\":\"%s\"}", id_str, script_name);

    /* Log Request */
    mlog(INFO, "request: %s at %s\n", id_str, script_name);

    /* Launch Engine */
    const char* script_pathname = sanitize(script_name);
    LuaEngine* engine = new LuaEngine(id_str, script_pathname, hm->body.p, trace_id, NULL, true);
    bool status = engine->executeEngine(MAX_RESPONSE_TIME_MS);

    /* Send Response */
    if(status)
    {
        const char* result = engine->getResult();
        if(result)
        {
            mg_send_head(nc, 200, strlen(result), "Content-Type: text/plain");
            mg_printf(nc, "%s", result);
        }
        else
        {
            mg_send_head(nc, 404, 0, NULL); // Not Found
        }
    }
    else
    {
        mg_send_head(nc, 408, 0, NULL); // Request Timeout
    }

    /* Clean Up */
    delete engine;
    delete [] script_pathname;
    delete [] script_name;

    /* Stop Trace */
    stop_trace(trace_id);
}

/*----------------------------------------------------------------------------
 * engineHandler
 *----------------------------------------------------------------------------*/
void MongooseServer::engineHandler (struct mg_connection *nc, struct http_message *hm)
{
    MongooseServer* server = (MongooseServer*)nc->mgr->user_data;
    char id_str[REQUEST_ID_LEN];
    getUniqueId(id_str, server->getName());

    /* Get Request Parmeters */
    const char* script_name = getEndpoint(hm->uri.p);
    if(!script_name) return;

    /* Start Trace */
    uint32_t parent_id = server->traceId;
    uint32_t trace_id = start_trace_ext(parent_id, "engine_handler", "{\"rqst_id\":\"%s\", \"script\":\"%s\"}", id_str, script_name);

    /* Log Request */
    mlog(INFO, "request: %s at %s\n", id_str, script_name);

    /* Create Engine */
    const char* script_pathname = sanitize(script_name);
    LuaEngine* engine = new LuaEngine(id_str, script_pathname, hm->body.p, trace_id, NULL, true);

    /* Supply and Setup Request Queue */
    engine->setString(RESPONSE_QUEUE, id_str);
    Subscriber rspq(id_str);

    /* Execute Engine
     *  the call to execute the script returns immediately (due to IO_CHECK) at which point
     *  the lua state context is locked and cannot be accessed until the script completes */
    engine->executeEngine(IO_CHECK);

    /* Send Response Header */
    const char* content_type = "application/octet-stream";
    const char* transfer_encoding = "chunked"; 
    mg_printf(nc, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nServer: sliderule/%s\r\nTransfer-Encoding: %s\r\n\r\n", content_type, BINID, transfer_encoding);
    mg_mgr_poll(nc->mgr, 0);
    
    /* Stream Response
     *  the response is read from the response queue until both the script completes and
     *  there are no more messages left in the message queue */
    int status = MsgQ::STATE_OKAY;
    while(engine->isActive() || status == MsgQ::STATE_OKAY)
    {
        Subscriber::msgRef_t ref;
        status = rspq.receiveRef(ref, SYS_TIMEOUT);
        if(status == MsgQ::STATE_OKAY)
        {            
            uint32_t size = ref.size;
            if(size > 0)
            {
                mg_send_http_chunk(nc, (const char*)&size, sizeof(size));
                mg_send_http_chunk(nc, (const char*)ref.data, ref.size);
                mg_mgr_poll(nc->mgr, 0);
            }
            else
            {
                mg_send_http_chunk(nc, "", 0); /* send empty chunk, the end of response */
            }
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            // do nothing
        }
        else
        {
            mlog(CRITICAL, "%s error streaming data: %d\n", id_str, status);
            break;
        }
    }

    /* Clean Up */
    delete engine;
    delete [] script_pathname;
    delete [] script_name;

    /* Stop Trace */
    stop_trace(trace_id);
}

/*----------------------------------------------------------------------------
 * serverHandler
 *----------------------------------------------------------------------------*/
void MongooseServer::serverHandler (struct mg_connection *nc, int ev, void *ev_data)
{
    struct http_message* hm = (struct http_message*)ev_data;
    switch (ev) 
    {
        case MG_EV_HTTP_REQUEST:
        {
            if(StringLib::find(hm->uri.p, "/source/") == hm->uri.p)
            {
                if(StringLib::find(hm->method.p, "GET"))
                {
                    sourceHandler(nc, hm);
                }
                else if(StringLib::find(hm->method.p, "POST"))
                {
                    engineHandler(nc, hm);
                }
            }
            break;
        }
        default: break;
    }
}

/*----------------------------------------------------------------------------
 * serverThread
 *----------------------------------------------------------------------------*/
void* MongooseServer::serverThread (void* parm)
{
    MongooseServer* server = (MongooseServer*)parm;

    struct mg_mgr           mgr;
    struct mg_connection*   nc;
    struct mg_bind_opts     bind_opts;
    const char*             err_str;

    mg_mgr_init(&mgr, NULL);
    mgr.user_data = (void*)server;

    memset(&bind_opts, 0, sizeof(bind_opts));
    bind_opts.error_string = &err_str;
    
    nc = mg_bind_opt(&mgr, server->port, serverHandler, bind_opts);
    if(nc)
    {
        mg_set_protocol_http_websocket(nc);
        while(server->active) 
        {
            mg_mgr_poll(&mgr, 1000);
        }
        mg_mgr_free(&mgr);
    }
    else
    {
        fprintf(stderr, "Error starting server on port %s: %s\n", server->port, *bind_opts.error_string);
    }

    return NULL;
}
