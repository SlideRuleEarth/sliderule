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
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* HttpServer::DURATION_METRIC = "duration";

const char* HttpServer::OBJECT_TYPE = "HttpServer";
const char* HttpServer::LuaMetaName = "HttpServer";
const struct luaL_Reg HttpServer::LuaMetaTable[] = {
    {"attach",      luaAttach},
    {"metric",      luaMetric},
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
        int         port            = (int)getLuaInteger(L, 1);
        const char* ip_addr         = getLuaString(L, 2, true, NULL);
        int         max_connections = (int)getLuaInteger(L, 3, true, DEFAULT_MAX_CONNECTIONS);

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
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    connections(max_connections)
{
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;

    active = true;
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

    if(ipAddr) delete [] ipAddr;

    EndpointObject* endpoint;
    const char* key = routeTable.first(&endpoint);
    while(key != NULL)
    {
        endpoint->releaseLuaObject();
        key = routeTable.next(&endpoint);
    }
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
const char* HttpServer::getUniqueId (void)
{
    char* id_str = new char [REQUEST_ID_LEN];
    long id = requestId++;
    StringLib::format(id_str, REQUEST_ID_LEN, "%s:%d:%ld", getIpAddr(), getPort(), id);
    return id_str;
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* HttpServer::getIpAddr (void)
{
    if(ipAddr)  return ipAddr;
    else        return "0.0.0.0";
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int HttpServer::getPort (void)
{
    return port;
}

/*----------------------------------------------------------------------------
 * listenerThread
 *----------------------------------------------------------------------------*/
void* HttpServer::listenerThread(void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    while(s->active)
    {
        /* Start Http Server */
        int status = SockLib::startserver(s->getIpAddr(), s->getPort(), DEFAULT_MAX_CONNECTIONS, pollHandler, activeHandler, &s->active, (void*)s);
        if(status < 0)
        {
            /* Set Global Health State */
            setunhealthy();
            mlog(CRITICAL, "Http server on %s:%d returned error: %d", s->getIpAddr(), s->getPort(), status);

            /* Restart Http Server */
            if(s->active)
            {
                mlog(INFO, "Attempting to restart http server: %s", s->getName());
                LocalLib::sleep(3.0); // wait three second to prevent spint
            }
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * extract
 *
 *  Note: must delete returned strings
 *----------------------------------------------------------------------------*/
void HttpServer::extract (const char* url, char** endpoint, char** new_url)
{
    const char* src;
    char* dst;

    *endpoint = NULL;
    *new_url = NULL;

    const char* first_slash = StringLib::find(url, '/');
    if(first_slash)
    {
        const char* second_slash = StringLib::find(first_slash+1, '/');
        if(second_slash)
        {
            /* Get Endpoint */
            int endpoint_len = second_slash - first_slash + 1; // this includes null terminator and slash
            *endpoint = new char[endpoint_len];
            src = first_slash ; // include the slash
            dst = *endpoint;
            while(src < second_slash) *dst++ = *src++;
            *dst = '\0';

            /* Get New URL */
            const char* terminator = StringLib::find(second_slash+1, '\0');
            if(terminator)
            {
                int new_url_len = terminator - second_slash; // this includes null terminator
                *new_url = new char[new_url_len];
                src = second_slash + 1; // do NOT include the slash
                dst = *new_url;
                while(src < terminator) *dst++ = *src++;
                *dst = '\0';
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * luaAttach - :attach(<EndpointObject>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaAttach (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        HttpServer* lua_obj = (HttpServer*)getLuaSelf(L, 1);

        /* Get Parameters */
        EndpointObject* endpoint = (EndpointObject*)getLuaObject(L, 2, EndpointObject::OBJECT_TYPE);
        const char* url = getLuaString(L, 3);

        /* Add Route to Table */
        status = lua_obj->routeTable.add(url, endpoint, true);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error attaching handler: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaMetric - :metric(<endpoint name>)
 *
 * Note: NOT thread safe, must be called before first request
 *----------------------------------------------------------------------------*/
int HttpServer::luaMetric (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        HttpServer* lua_obj = (HttpServer*)getLuaSelf(L, 1);

        /* Get Object Name */
        const char* obj_name = lua_obj->getName();

        /* Register Metrics */
        lua_obj->metricId = EventLib::registerMetric(obj_name, EventLib::COUNTER, "%s", DURATION_METRIC);
        if(lua_obj->metricId == EventLib::INVALID_METRIC)
        {
            throw RunTimeException(ERROR, "Registry failed for %s.%s", obj_name, DURATION_METRIC);
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
 * pollHandler
 *
 *  Notes: provides the events back to the poll function
 *----------------------------------------------------------------------------*/
int HttpServer::pollHandler(int fd, short* events, void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    /* Get Connection */
    connection_t* connection = s->connections[fd];

    /* Set Polling Flags */
    *events |= IO_READ_FLAG;
    if(connection->state.ref_status > 0)
    {
        *events |= IO_WRITE_FLAG;
    }
    else
    {
        *events &= ~IO_WRITE_FLAG;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * activeHandler
 *
 *  Notes: performed on activity returned from poll
 *----------------------------------------------------------------------------*/
int HttpServer::activeHandler(int fd, int flags, void* parm)
{
    HttpServer* s = (HttpServer*)parm;

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
    char msg_buf[REQUEST_MSG_BUF_LEN];
    connection_t* connection = connections[fd];

    int bytes = SockLib::sockrecv(fd, msg_buf, REQUEST_MSG_BUF_LEN - 1, IO_CHECK);
    if(bytes > 0)
    {
        msg_buf[bytes] = '\0';
        connection->message += msg_buf;
        status = bytes;

        /* Look Through Existing Header Received */
        while(!connection->state.header_complete && (connection->state.header_index < (connection->message.getLength() - 4)))
        {
            /* If Header Complete (look for \r\n\r\n separator) */
            if( (connection->message[connection->state.header_index + 0] == '\r') &&
                (connection->message[connection->state.header_index + 1] == '\n') &&
                (connection->message[connection->state.header_index + 2] == '\r') &&
                (connection->message[connection->state.header_index + 3] == '\n') )
            {
                /* Parse Request */
                connection->message.setChar('\0', connection->state.header_index);
                List<SafeString>* header_list = connection->message.split('\r');
                connection->message.setChar('\r', connection->state.header_index);
                connection->state.header_complete = true;
                connection->state.header_index += 4; // moves state.header_index to start of body

                /* Parse Request Line */
                try
                {
                    List<SafeString>* request_line = (*header_list)[0].split(' ');
                    const char* verb_str = (*request_line)[0].getString();
                    const char* url_str = (*request_line)[1].getString();

                    /* Get Verb */
                    connection->request.verb = EndpointObject::str2verb(verb_str);

                    /* Get Endpoint and URL */
                    char* endpoint = NULL;
                    extract(url_str, &endpoint, &connection->request.url);
                    if(endpoint && connection->request.url)
                    {
                        try
                        {
                            /* Get Attached Endpoint Object */
                            connection->request.endpoint = routeTable[endpoint];
                        }
                        catch(const RunTimeException& e)
                        {
                            mlog(e.level(), "No attached endpoint at %s: %s", endpoint, e.what());
                            status = INVALID_RC; // will close socket
                        }
                    }
                    else
                    {
                        mlog(CRITICAL, "Unable to extract endpoint and url: %s", url_str);
                    }

                    /* Clean Up Allocated Memory */
                    delete request_line;
                    if(endpoint) delete [] endpoint;
                }
                catch(const RunTimeException& e)
                {
                    mlog(e.level(), "Invalid request line: %s", e.what());
                }

                /* Parse Headers */
                for(int h = 1; h < header_list->length(); h++)
                {
                    /* Create Key/Value Pairs */
                    List<SafeString>* keyvalue_list = (*header_list)[h].split(':');
                    try
                    {
                        const char* key = (*keyvalue_list)[0].getString();
                        const char* value = (*keyvalue_list)[1].getString(true);
                        connection->request.headers->add(key, value, true);
                    }
                    catch(const RunTimeException& e)
                    {
                        mlog(e.level(), "Invalid header in http request: %s: %s", (*header_list)[h].getString(), e.what());
                    }
                    delete keyvalue_list;
                }

                /* Clean Up Header List */
                delete header_list;

                /* Get Content Length */
                try
                {
                    if(!StringLib::str2long(connection->request.headers->get("Content-Length"), &connection->request.body_length))
                    {
                        mlog(CRITICAL, "Invalid Content-Length header: %s", connection->request.headers->get("Content-Length"));
                        status = INVALID_RC; // will close socket
                    }
                }
                catch(const RunTimeException& e)
                {
                    connection->request.body_length = 0;
                }
            }
            else
            {
                /* Go to Next Character in Header */
                connection->state.header_index++;
            }
        }

        /* Check If Body Complete */
        if((connection->message.getLength() - connection->state.header_index) >= connection->request.body_length)
        {
            /* Get Message Body */
            if(connection->request.body_length > 0)
            {
                const char* raw_message = connection->message.getString();
                connection->request.body = &raw_message[connection->state.header_index];
            }
            else
            {
                connection->request.body = NULL;
            }

            /* Handle Request */
            if(connection->request.endpoint)
            {
                connection->request.response_type = connection->request.endpoint->handleRequest(&connection->request);
            }
            else // currently no other types of handers
            {
                mlog(CRITICAL, "Unable to handle unattached request");
                status = INVALID_RC; // will close socket
            }
        }
    }
    else
    {
        /* Failed to receive data on socket that was marked for reading */
        status = INVALID_RC; // will close socket
    }

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
    connection_t* connection = connections[fd];
    bool ref_complete = false;

    uint8_t* buffer;
    int bytes_left;

    /* If Something to Send */
    if(connection->state.ref_status > 0)
    {
        if(connection->state.header_sent && connection->request.response_type == EndpointObject::STREAMING) /* Setup Streaming */
        {
            /* Allocate Streaming Buffer (if necessary) */
            if(connection->state.ref.size + STREAM_OVERHEAD_SIZE > connection->state.stream_mem_size)
            {
                /* Delete Old Buffer */
                if(connection->state.stream_buf) delete [] connection->state.stream_buf;

                /* Allocate New Buffer */
                connection->state.stream_mem_size = connection->state.ref.size + STREAM_OVERHEAD_SIZE;
                connection->state.stream_buf = new uint8_t [connection->state.stream_mem_size];
            }

            /* Build Stream Buffer */
            if(connection->state.stream_buf_size == 0)
            {
                /* Write Chunk Header - HTTP */
                unsigned long chunk_size = connection->state.ref.size > 0 ? connection->state.ref.size + sizeof(uint32_t) : 0;
                StringLib::format((char*)connection->state.stream_buf, STREAM_OVERHEAD_SIZE, "%lX\r\n", chunk_size);
                connection->state.stream_buf_size = StringLib::size((const char*)connection->state.stream_buf, connection->state.stream_mem_size);

                if(connection->state.ref.size > 0)
                {
                    /* Write Message Size */
                    #ifdef __be__
                    uint32_t rec_size = LocalLib::swapl(rec_size);
                    #else
                    uint32_t rec_size = connection->state.ref.size;
                    #endif
                    LocalLib::copy(&connection->state.stream_buf[connection->state.stream_buf_size], &rec_size, sizeof(uint32_t));
                    connection->state.stream_buf_size += sizeof(uint32_t);

                    /* Write Message Data */
                    LocalLib::copy(&connection->state.stream_buf[connection->state.stream_buf_size], connection->state.ref.data, connection->state.ref.size);
                    connection->state.stream_buf_size += connection->state.ref.size;
                }

                /* Write Chunk Trailer - HTTP */
                StringLib::format((char*)&connection->state.stream_buf[connection->state.stream_buf_size], STREAM_OVERHEAD_SIZE, "\r\n");
                connection->state.stream_buf_size += 2;
            }

            /* Setup Write State */
            buffer = &connection->state.stream_buf[connection->state.stream_buf_index];
            bytes_left = connection->state.stream_buf_size - connection->state.stream_buf_index;
        }
        else /* Setup Normal */
        {
            /* Setup Write State */
            buffer = ((uint8_t*)connection->state.ref.data) + connection->state.ref_index;
            bytes_left = connection->state.ref.size - connection->state.ref_index;
        }

        /* If Anything Left to Send */
        if(bytes_left > 0)
        {
            /* Write Data to Socket */
            int bytes = SockLib::socksend(fd, buffer, bytes_left, IO_CHECK);
            if(bytes >= 0)
            {
                /* Update Status */
                status += bytes;

                /* Update Write State */
                if(connection->state.header_sent && connection->request.response_type == EndpointObject::STREAMING)
                {
                    /* Update Streaming Write State */
                    connection->state.stream_buf_index += bytes;
                    if(connection->state.stream_buf_index == connection->state.stream_buf_size)
                    {
                        connection->state.stream_buf_index = 0;
                        connection->state.stream_buf_size = 0;
                        ref_complete = true;
                    }
                }
                else
                {
                    /* Update Normal Write State
                    *  note that this code will be executed once for the
                    *  header of a streaming write as well */
                    connection->state.ref_index += bytes;
                    if(connection->state.ref_index == connection->state.ref.size)
                    {
                        connection->state.header_sent = true;
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
        if(connection->state.ref.size == 0)
        {
            ref_complete = true; // logic is skipped above on terminating message
            connection->state.response_complete = true; // prevent further messages received
            status = INVALID_RC; // will close socket
        }

        /* Reset State */
        if(ref_complete)
        {
            connection->state.rspq->dereference(connection->state.ref);
            connection->state.ref_status = 0;
            connection->state.ref_index = 0;
            connection->state.ref.size = 0;
        }
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onAlive
 *
 *  Notes: Performed for every existing connection
 *----------------------------------------------------------------------------*/
int HttpServer::onAlive(int fd)
{
    connection_t* connection = connections[fd];

    if(!connection->state.response_complete && connection->state.ref_status <= 0)
    {
        connection->state.ref_status = connection->state.rspq->receiveRef(connection->state.ref, IO_CHECK);
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
    connection_t* connection = new connection_t;
    LocalLib::set(&connection->request, 0, sizeof(EndpointObject::request_t));
    LocalLib::set(&connection->state, 0, sizeof(state_t));
    connection->start_time = TimeLib::latchtime();

    /* Register Connection */
    if(connections.add(fd, connection, false))
    {
        connection->request.id = getUniqueId(); // id is allocated
        connection->request.headers = new Dictionary<const char*>(EXPECTED_MAX_HEADER_FIELDS);
        connection->state.rspq = new Subscriber(connection->request.id);
    }
    else
    {
        mlog(CRITICAL, "HTTP server at %s failed to register connection due to duplicate entry", connection->request.id);
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

    connection_t* connection = connections[fd];

    /* Update Metrics */
    double duration = TimeLib::latchtime() - connection->start_time;
    EventLib::incrementMetric(metricId, duration);

    /* Remove Connection */
    if(connections.remove(fd))
    {
        /* Clear Out Headers */
        const char* header;
        const char* key = connection->request.headers->first(&header);
        while(key != NULL)
        {
            /* Free Header Value */
            delete [] header;
            key = connection->request.headers->next(&header);
        }

        /* Free URL */
        if(connection->request.url) delete [] connection->request.url;

        /* Free Stream Buffer */
        if(connection->state.stream_buf) delete [] connection->state.stream_buf;

        /* Free Rest of Allocated Connection Members */
        delete [] connection->request.id;
        delete connection->request.headers;
        delete connection->state.rspq;
        delete connection->request.pid;

        /* Free Connection */
        delete connection;
    }
    else
    {
        mlog(CRITICAL, "HTTP server at %s failed to release connection", connection->request.id);
        status = INVALID_RC;
    }

    return status;
}
