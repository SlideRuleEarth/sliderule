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

#include <atomic>

#include "HttpServer.h"
#include "OsApi.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* HttpServer::OBJECT_TYPE = "HttpServer";
const char* HttpServer::LUA_META_NAME = "HttpServer";
const struct luaL_Reg HttpServer::LUA_META_TABLE[] = {
    {"attach",      luaAttach},
    {"untilup",     luaUntilUp},
    {NULL,          NULL}
};

std::atomic<uint64_t> HttpServer::requestId{0};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - server(<ip_addr>, <port>, [<max connections>])
 *----------------------------------------------------------------------------*/
int HttpServer::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const int   port            = (int)getLuaInteger(L, 1);
        const char* ip_addr         = getLuaString(L, 2, true, NULL);
        const int   max_connections = (int)getLuaInteger(L, 3, true, DEFAULT_MAX_CONNECTIONS);

        /* Get Server Parameter */
        if( ip_addr && (StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*")) )
        {
            ip_addr = NULL;
        }

        /* Return File Device Object */
        return createLuaObject(L, new HttpServer(L, ip_addr, port, max_connections));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating HttpServer: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
HttpServer::HttpServer(lua_State* L, const char* _ip_addr, int _port, int max_connections):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    connections(max_connections)
{
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;

    active = true;
    listening = false;
    listenerPid = new Thread(listenerThread, this);

    metricId = EventLib::INVALID_METRIC;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
HttpServer::~HttpServer(void)
{
    active = false;
    delete listenerPid;
    delete [] ipAddr;
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* HttpServer::getIpAddr (void) const
{
    if(ipAddr) return ipAddr;
    return "0.0.0.0";
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int HttpServer::getPort (void) const
{
    return port;
}

/*----------------------------------------------------------------------------
 * Connection Constructor
 *----------------------------------------------------------------------------*/
HttpServer::Connection::Connection (const char* _name)
{
    initialize(_name);
    memset(&rqst_state, 0, sizeof(rqst_state_t));
}

/*----------------------------------------------------------------------------
 * Connection Copy Constructor
 *----------------------------------------------------------------------------*/
HttpServer::Connection::Connection (const Connection& other)
{
    initialize(other.name);
    memcpy(&rqst_state, &other.rqst_state, sizeof(rqst_state_t));
}

/*----------------------------------------------------------------------------
 * Connection Destructor
 *----------------------------------------------------------------------------*/
HttpServer::Connection::~Connection (void)
{
    /* Free Stream Buffer */
    delete [] rsps_state.stream_buf;

    /* Free Message Queue */
    if(rsps_state.ref_status > 0)
    {
        rsps_state.rspq->dereference(rsps_state.ref);
        rsps_state.ref_status = 0;
    }
    delete rsps_state.rspq;

    /* Free Id */
    delete [] id;
    delete [] name;

    /* Request freed only if present, o/w memory owned by EndpointObject */
    delete request;

    /* Stop Trace */
    stop_trace(DEBUG, trace_id);
}

/*----------------------------------------------------------------------------
 * Connection Initialization
 *----------------------------------------------------------------------------*/
void HttpServer::Connection::initialize (const char* _name)
{
    /* Initialize Response State Variables */
    memset(&rsps_state, 0, sizeof(rsps_state_t));
    response_type = EndpointObject::INVALID;

    /* Default Keep Alive to False */
    keep_alive = false;

    /* Create Unique ID for Request */
    name = StringLib::duplicate(_name);
    id = new char [REQUEST_ID_LEN];
    const long cnt = requestId++;
    StringLib::format(id, REQUEST_ID_LEN, "%s.%ld", name, cnt);

    /* Start Trace */
    trace_id = start_trace(DEBUG, ORIGIN, "http_server", "{\"rqst_id\":\"%s\"}", id);

    /* Subscribe to Response Q (data return by endpoint) */
    rsps_state.rspq = new Subscriber(id);

    /* Create Request */
    request = new EndpointObject::Request(id);
    request->trace_id = trace_id;
}

/*----------------------------------------------------------------------------
 * extractPath
 *
 *  Note: must delete returned strings
 *----------------------------------------------------------------------------*/
void HttpServer::extractPath (const char* url, const char** path, const char** resource)
{
    *path = NULL;
    *resource = NULL;

    const char* first_slash = StringLib::find(url, '/');
    if(first_slash)
    {
        const char* second_slash = StringLib::find(first_slash+1, '/');
        if(second_slash)
        {
            /* Get Endpoint */
            const int path_len = second_slash - first_slash + 1; // this includes null terminator and slash
            char*       dst = new char[path_len];
            const char* src = first_slash ; // include the slash
            *path = dst;
            while(src < second_slash) *dst++ = *src++;
            *dst = '\0';

            /* Get New URL */
            const char* terminator = StringLib::find(second_slash+1, '\0');
            if(terminator)
            {
                const int resource_len = terminator - second_slash; // this includes null terminator
                dst = new char[resource_len];
                src = second_slash + 1; // do NOT include the slash
                *resource = dst;
                while(src < terminator) *dst++ = *src++;
                *dst = '\0';
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * processHttpHeader
 *----------------------------------------------------------------------------*/
bool HttpServer::processHttpHeader (char* buf, EndpointObject::Request* request)
{
    bool status = true;

    List<string*>* header_list = NULL;
    List<string*>* request_line = NULL;
    try
    {
        /* Parse Request */
        const string http_header(buf);
        header_list = StringLib::split(http_header.c_str(), http_header.length(), '\r');
        request_line = StringLib::split((*header_list)[0]->c_str(), (*header_list)[0]->length(), ' ');

        const char* verb_str = (*request_line)[0]->c_str();
        const char* url_str = (*request_line)[1]->c_str();
        const char* version = (*request_line)[2]->c_str();

        /* Get Verb */
        request->verb = EndpointObject::str2verb(verb_str);
        if(request->verb == EndpointObject::UNRECOGNIZED)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "unrecognized HTTP verb: %s", verb_str);
        }

        /* Get Version */
        request->version = StringLib::duplicate(version);
        if(request->version == NULL)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR,  "no HTTP version specified");
        }

        /* Get Endpoint and URL */
        extractPath(url_str, &request->path, &request->resource);
        if(!request->path || !request->resource)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "unable to extract endpoint and url: %s", url_str);
        }

        /* Parse Headers */
        for(int h = 1; h < header_list->length(); h++)
        {
            /* Create Key/Value Pairs */
            List<string*>* keyvalue_list = StringLib::split(header_list->get(h)->c_str(), header_list->get(h)->length(), ':');
            try
            {
                char* key = const_cast<char*>((*keyvalue_list)[0]->c_str());
                string* value = new string(*(*keyvalue_list)[1]);
                StringLib::convertLower(key);
                if(!request->headers.add(key, value, true)) delete value;
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Invalid header in http request: %s: %s", header_list->get(h)->c_str(), e.what());
            }
            delete keyvalue_list;
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Invalid request: %s", e.what());
        status = false;
    }

    /* Clean Up Allocated Memory */
    delete request_line;
    delete header_list;

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * listenerThread
 *----------------------------------------------------------------------------*/
void* HttpServer::listenerThread(void* parm)
{
    HttpServer* s = static_cast<HttpServer*>(parm);

    while(s->active)
    {
        /* Start Http Server */
        const int status = SockLib::startserver(s->getIpAddr(), s->getPort(), DEFAULT_MAX_CONNECTIONS, pollHandler, activeHandler, &s->active, static_cast<void*>(s), &(s->listening));
        if(status < 0)
        {
            mlog(CRITICAL, "Http server on %s:%d returned error: %d", s->getIpAddr(), s->getPort(), status);
            s->listening = false;

            /* Restart Http Server */
            if(s->active)
            {
                mlog(INFO, "Attempting to restart http server: %s", s->getName());
                OsApi::sleep(5.0); // wait five seconds to prevent spin
            }
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * pollHandler
 *
 *  Notes: provides the events back to the poll function
 *----------------------------------------------------------------------------*/
int HttpServer::pollHandler(int fd, short* events, void* parm)
{
    HttpServer* s = static_cast<HttpServer*>(parm);

    /* Get Connection */
    Connection* connection = s->connections[fd];
    const rsps_state_t* state = &connection->rsps_state;

    /* Set Read Polling Flag (if request is ready) */
    if(connection->request) *events |= IO_READ_FLAG;
    else *events &= ~IO_READ_FLAG;

    /* Set Write Polling Flag (if data to write) */
    if(state->ref_status > 0) *events |= IO_WRITE_FLAG;
    else *events &= ~IO_WRITE_FLAG;

    return 0;
}

/*----------------------------------------------------------------------------
 * activeHandler
 *
 *  Notes: performed on activity returned from poll
 *----------------------------------------------------------------------------*/
int HttpServer::activeHandler(int fd, int flags, void* parm)
{
    HttpServer* s = static_cast<HttpServer*>(parm);

    int rc = 0;

    if((flags & IO_READ_FLAG)       && (s->onRead(fd)       < 0))   rc = INVALID_RC;
    if((flags & IO_WRITE_FLAG)      && (s->onWrite(fd)      < 0))   rc = INVALID_RC;
    if((flags & IO_ALIVE_FLAG)      && (s->onAlive(fd)      < 0))   rc = INVALID_RC;
    if((flags & IO_CONNECT_FLAG)    && (s->onConnect(fd)    < 0))   rc = INVALID_RC;
    if((flags & IO_DISCONNECT_FLAG) && (s->onDisconnect(fd) < 0))   rc = INVALID_RC;

    return rc;
}

/*----------------------------------------------------------------------------
 * onRead
 *
 *  Notes: performed for every connection that is ready to have data read from it
 *----------------------------------------------------------------------------*/
int HttpServer::onRead(int fd)
{
    int status = 0;
    Connection* connection = connections[fd];
    rqst_state_t* state = &connection->rqst_state;
    const uint32_t trace_id = start_trace(DEBUG, connection->trace_id, "on_read", "%s", "{}");

    /* Determine Buffer to Read Into */
    uint8_t* buf; // pointer to buffer to read into
    int buf_available; // bytes available in buffer
    if(!state->header_complete)
    {
        buf = reinterpret_cast<uint8_t*>(&state->header_buf[state->header_size]);
        buf_available = HEADER_BUF_LEN - state->header_size;
    }
    else if(connection->request->body)
    {
        buf = &connection->request->body[state->body_size];
        buf_available = connection->request->length - state->body_size + 1;
    }
    else
    {
        // Early Exit - Error Condition
        return INVALID_RC;
    }

    /* Socket Read */
    const int bytes = SockLib::sockrecv(fd, buf, buf_available - 1, IO_CHECK);
    if(bytes > 0)
    {
        status = bytes;

        /* Update Buffer Size */
        if(!state->header_complete)
        {
            state->header_size += bytes;
        }
        else
        {
            state->body_size += bytes;
        }

        /* Look Through Existing Header Received */
        while(!state->header_complete && (state->header_index <= (state->header_size - 4)))
        {
            /* If Header Complete (look for \r\n\r\n separator) */
            if( (state->header_buf[state->header_index + 0] == '\r') &&
                (state->header_buf[state->header_index + 1] == '\n') &&
                (state->header_buf[state->header_index + 2] == '\r') &&
                (state->header_buf[state->header_index + 3] == '\n') )
            {
                state->header_buf[state->header_index] = '\0';
                state->header_complete = true;
                state->header_index += 4;

                /* Process HTTP Header */
                if(processHttpHeader(state->header_buf, connection->request))
                {
                    /* Get Content Length */
                    try
                    {
                        if(StringLib::str2long(connection->request->headers["content-length"]->c_str(), &connection->request->length))
                        {
                            /* Allocate and Prepopulate Request Body */
                            connection->request->body = new uint8_t[connection->request->length + 1];
                            connection->request->body[connection->request->length] = '\0';
                            const int bytes_to_copy = state->header_size - state->header_index;
                            memcpy(connection->request->body, &state->header_buf[state->header_index], bytes_to_copy);
                            state->body_size += bytes_to_copy;
                        }
                        else
                        {
                            mlog(CRITICAL, "Invalid Content-Length header: %s", connection->request->headers["content-length"]->c_str());
                            status = INVALID_RC; // will close socket
                        }
                    }
                    catch(const RunTimeException& e)
                    {
                        connection->request->length = 0;
                    }

                    /* Get Keep Alive Setting based on HTTP version
                     * note that HTTP/1.0 defaults to close and HTTP/1.1 defaults to keep-alive */
                    try
                    {
                        if(StringLib::match(connection->request->version, "HTTP/1.0"))
                        {
                            connection->keep_alive = false;
                            if(StringLib::match(connection->request->headers["connection"]->c_str(), "keep-alive"))
                                connection->keep_alive = true;
                        }
                        else if(StringLib::match(connection->request->version, "HTTP/1.1"))
                        {
                            connection->keep_alive = true;
                            if(StringLib::match(connection->request->headers["connection"]->c_str(), "close"))
                                connection->keep_alive = false;
                        }
                        else
                        {
                            mlog(CRITICAL, "Unsupported HTTP version: %s", connection->request->version);
                            connection->keep_alive = false;
                        }
                    }
                    catch(const RunTimeException& e)
                    {
                        (void)e;
                    }
                }
                else
                {
                    status = INVALID_RC; // will close socket
                }
            }
            else
            {
                /* Go to Next Character in Header */
                state->header_index++;
            }
        }

        /* Check If Body Complete */
        if(state->header_complete && (state->body_size >= connection->request->length) && (status >= 0))
        {
            /* Handle Request */
            const char* path = connection->request->path;
            try
            {
                EndpointObject* endpoint = routeTable[path]->route;
                connection->response_type = endpoint->handleRequest(connection->request);
                connection->request = NULL; // no longer owned by HttpServer, owned by EndpointObject
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "No attached endpoint at %s: %s", path, e.what());
                status = INVALID_RC; // will close socket
            }
            memset(&connection->rqst_state, 0, sizeof(rqst_state_t));
        }
    }
    else
    {
        /* Failed to receive data on socket that was marked for reading */
        status = INVALID_RC; // will close socket
    }

    /* Stop Trace */
    stop_trace(DEBUG, trace_id);

    return status;
}

/*----------------------------------------------------------------------------
 * onWrite
 *
 *  Notes: performed for every request that is ready to have data written to it
 *----------------------------------------------------------------------------*/
int HttpServer::onWrite(int fd)
{
    int status = 0;
    Connection* connection = connections[fd];
    rsps_state_t* state = &connection->rsps_state;
    const uint32_t trace_id = start_trace(DEBUG, connection->trace_id, "on_write", "%s", "{}");

    /* If Something to Send */
    if(state->ref_status > 0)
    {
        bool ref_complete = false;
        const uint8_t* buffer;
        int bytes_left;

        if(state->header_sent && connection->response_type == EndpointObject::STREAMING) /* Setup Streaming */
        {
            /* Allocate Streaming Buffer (if necessary) */
            if(state->ref.size + STREAM_OVERHEAD_SIZE > state->stream_mem_size)
            {
                /* Delete Old Buffer */
                delete [] state->stream_buf;

                /* Allocate New Buffer */
                state->stream_mem_size = state->ref.size + STREAM_OVERHEAD_SIZE;
                state->stream_buf = new uint8_t [state->stream_mem_size];
            }

            /* Build Stream Buffer */
            if(state->stream_buf_size == 0)
            {
                /* Write Chunk Header - HTTP */
                const unsigned long chunk_size = state->ref.size > 0 ? state->ref.size : 0;
                StringLib::format(reinterpret_cast<char*>(state->stream_buf), STREAM_OVERHEAD_SIZE, "%lX\r\n", chunk_size);
                state->stream_buf_size = StringLib::size(reinterpret_cast<const char*>(state->stream_buf));

                if(state->ref.size > 0)
                {
                    /* Write Message Data */
                    memcpy(&state->stream_buf[state->stream_buf_size], state->ref.data, state->ref.size);
                    state->stream_buf_size += state->ref.size;
                }

                /* Write Chunk Trailer - HTTP */
                StringLib::format(reinterpret_cast<char*>(&state->stream_buf[state->stream_buf_size]), STREAM_OVERHEAD_SIZE, "\r\n");
                state->stream_buf_size += 2;
            }

            /* Setup Write State */
            buffer = &state->stream_buf[state->stream_buf_index];
            bytes_left = state->stream_buf_size - state->stream_buf_index;
        }
        else /* Setup Normal */
        {
            /* Setup Write State */
            buffer = reinterpret_cast<uint8_t*>(state->ref.data) + state->ref_index;
            bytes_left = state->ref.size - state->ref_index;
        }

        /* If Anything Left to Send */
        if(bytes_left > 0)
        {
            /* Write Data to Socket */
            const int bytes = SockLib::socksend(fd, buffer, bytes_left, IO_CHECK);
            if(bytes >= 0)
            {
                /* Update Status */
                status += bytes;

                /* Update Write State */
                if(state->header_sent && connection->response_type == EndpointObject::STREAMING)
                {
                    /* Update Streaming Write State */
                    state->stream_buf_index += bytes;
                    if(state->stream_buf_index == state->stream_buf_size)
                    {
                        state->stream_buf_index = 0;
                        state->stream_buf_size = 0;
                        ref_complete = true;
                    }
                }
                else
                {
                    /* Update Normal Write State
                    *  note that this code will be executed once for the
                    *  header of a streaming write as well */
                    state->ref_index += bytes;
                    if(state->ref_index == state->ref.size)
                    {
                        state->header_sent = true;
                        ref_complete = true;
                    }
                }
            }
            else
            {
                /* Failed to Write Ready Socket */
                status = INVALID_RC; // will close socket
            }
        }

        /* Check if Done with Entire Response
         *  a valid reference of size zero indicates that
         *  the response is complete */
        if(state->ref.size == 0)
        {
            ref_complete = true; // logic is skipped above on terminating message
            state->response_complete = true; // prevent further messages received
            status = INVALID_RC; // will close socket
        }

        /* Reset State */
        if(ref_complete)
        {
            state->rspq->dereference(state->ref);
            state->ref_status = 0;
            state->ref_index = 0;
            state->ref.size = 0;
        }

        /* Check for Keep Alive */
        if(state->response_complete && connection->keep_alive)
        {
            Connection* new_connection = new Connection(*connection);
            const bool rc = connections.add(fd, new_connection, false); // deletes old connection
            if(rc)
            {
                status = 0; // will keep socket open
            }
            else
            {
                mlog(CRITICAL, "Failed to keep connection open due to table error");
                status = INVALID_RC; // will close socket
                delete new_connection; // free memory that would otherwise be leaked
            }
        }
    }

    /* Stop Trace */
    stop_trace(DEBUG, trace_id);

    return status;
}

/*----------------------------------------------------------------------------
 * onAlive
 *
 *  Notes: Performed for every existing connection
 *----------------------------------------------------------------------------*/
int HttpServer::onAlive(int fd)
{
    Connection* connection = connections[fd];
    rsps_state_t* state = &connection->rsps_state;

    if(!state->response_complete && state->ref_status <= 0)
    {
        state->ref_status = state->rspq->receiveRef(state->ref, IO_CHECK);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * onConnect
 *
 *  Notes: performed on new connections when the connection is made
 *----------------------------------------------------------------------------*/
int HttpServer::onConnect(int fd)
{
    int status = 0;

    /* Create and Initialize New Request */
    Connection* connection = new Connection(getName());

    /* Register Connection */
    if(!connections.add(fd, connection, true))
    {
        mlog(CRITICAL, "HTTP server at %s failed to register connection due to duplicate entry", connection->id);
        delete connection;
        status = INVALID_RC;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onDisconnect
 *
 *  Notes: performed on disconnected connections
 *----------------------------------------------------------------------------*/
int HttpServer::onDisconnect(int fd)
{
    int status = 0;

    Connection* connection = connections[fd];

    /* Remove Connection */
    if(!connections.remove(fd))
    {
        mlog(CRITICAL, "HTTP server at %s failed to release connection", connection->id);
        status = INVALID_RC;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * luaAttach - :attach(<EndpointObject>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaAttach (lua_State* L)
{
    bool status = false;
    EndpointObject* endpoint = NULL;

    try
    {
        /* Get Self */
        HttpServer* lua_obj = dynamic_cast<HttpServer*>(getLuaSelf(L, 1));

        /* Get Parameters */
        endpoint = dynamic_cast<EndpointObject*>(getLuaObject(L, 2, EndpointObject::OBJECT_TYPE));
        const char* url = getLuaString(L, 3);

        /* Add Route to Table */
        RouteEntry* entry = new RouteEntry(endpoint);
        status = lua_obj->routeTable.add(url, entry, true);
        if(!status) delete entry;
    }
    catch(const RunTimeException& e)
    {
        if(endpoint) endpoint->releaseLuaObject();
        mlog(e.level(), "Error attaching handler: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaUntilUp - :untilup(<seconds to wait>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaUntilUp (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        const HttpServer* lua_obj = dynamic_cast<HttpServer*>(getLuaSelf(L, 1));

        /* Get Parameters */
        int timeout = getLuaInteger(L, 2, true, IO_PEND);

        /* Wait Until Http Server Started */
        do
        {
            status = lua_obj->listening;
            if(status) break;
            if(timeout > 0) timeout--;
            OsApi::performIOTimeout();
        }
        while((timeout == IO_PEND) || (timeout > 0));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error waiting until HTTP server started: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
