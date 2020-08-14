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

#include <atomic>

#include "HttpServer.h"
#include "Ordering.h"
#include "List.h"
#include "LogLib.h"
#include "OsApi.h"
#include "StringLib.h"
#include "EndpointObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* HttpServer::OBJECT_TYPE = "HttpServer";
const char* HttpServer::LuaMetaName = "HttpServer";
const struct luaL_Reg HttpServer::LuaMetaTable[] = {
    {NULL,          NULL}
};

std::atomic<uint64_t> HttpServer::requestId{0};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - server(<ip_addr>, <port>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        int         port      = (int)getLuaInteger(L, 1);
        const char* ip_addr   = getLuaString(L, 2, true, NULL);

        /* Get Server Parameter */
        if( ip_addr && (StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*")) )
        {
            ip_addr = NULL;
        }

        /* Return File Device Object */
        return createLuaObject(L, new HttpServer(L, ip_addr, port));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating HttpServer: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
HttpServer::HttpServer(lua_State* L, const char* _ip_addr, int _port):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;

    active = true;
    listenerPid = new Thread(listenerThread, this);

    dataToWrite = false;
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
    char* id_str = new char [MAX_STR_SIZE];
    long id = requestId++;
    StringLib::format(id_str, REQUEST_ID_LEN, "%s:%d:%ld", getIpAddr(), getPort(), id);
    return id_str;
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* HttpServer::getIpAddr (void)
{
    return ipAddr;
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

    int status = SockLib::startserver (s->getIpAddr(), s->getPort(), IO_INFINITE_CONNECTIONS, pollHandler, activeHandler, (void*)s);
    if(status < 0) mlog(CRITICAL, "Failed to establish http server on %s:%d (%d)\n", s->getIpAddr(), s->getPort(), status);

    return NULL;
}

/*----------------------------------------------------------------------------
 * handlerThread
 *----------------------------------------------------------------------------*/
void* HttpServer::handlerThread(void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    return NULL;
}

/*----------------------------------------------------------------------------
 * extract
 * 
 *  Note: must delete returned strings
 *----------------------------------------------------------------------------*/
void HttpServer::extract (const char* url, const char** endpoint, const char** new_url)
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
            int endpoint_len = second_slash - first_slash; // this includes null terminator
            *endpoint = new char[endpoint_len];
            src = first_slash + 1;
            dst = *endpoint;
            while(src < second_slash) *dst++ = *src++;
            *dst = '\0';

            /* Get New URL */
            const char* first_space = StringLib::find(second_slash+1, ' ');
            if(first_space)
            {
                int new_url_len = first_space - second_slash; // this includes null terminator
                *new_url = new char[new_url_len];
                src = second_slash + 1;
                dst = *new_url;
                while(src < first_space) *dst++ = *src++;
                *dst = '\0';
            }
        }
    }
}

/*----------------------------------------------------------------------------
 * luaAttach - :attach(<EndpointObject>)
 *----------------------------------------------------------------------------*/
int HttpServer::luaAttach (lua_State L)
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

        /* Check Need to Unlock Unregistered Endpoint */
        if(status != true)
        {
            endpoint->releaseLuaObject();
        }
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error attaching handler: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * pollHandler
 *
 *  Notes: provides the flags back to the poll function
 *----------------------------------------------------------------------------*/
int HttpServer::pollHandler(int* flags, void* parm)
{
    HttpServer* s = (HttpServer*)parm;

    /* Check Alive */
    if(!s->active) return -1;

    /* Set Polling Flags */
    int pollflags = IO_READ_FLAG;
    if(s->dataToWrite) pollflags |= IO_WRITE_FLAG;
    s->dataToWrite = false;

    /* Return Polling Flags */
    *flags = pollflags;

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

    if((flags & IO_ALIVE_FLAG)      && (s->onAlive(fd)      < 0))   rc = -1;
    if((flags & IO_READ_FLAG)       && (s->onRead(fd)       < 0))   rc = -1;
    if((flags & IO_WRITE_FLAG)      && (s->onWrite(fd)      < 0))   rc = -1;
    if((flags & IO_CONNECT_FLAG)    && (s->onConnect(fd)    < 0))   rc = -1;
    if((flags & IO_DISCONNECT_FLAG) && (s->onDisconnect(fd) < 0))   rc = -1;

    return rc;
}

/*----------------------------------------------------------------------------
 * onRead
 *
 *  Notes: performed for every request that is ready to have data read from it
 *----------------------------------------------------------------------------*/
int HttpServer::onRead(int fd)
{
    int status = 0;
    char msg_buf[REQUEST_MSG_BUF_LEN];
    request_t* request = requests[fd];

    int bytes = SockLib::sockrecv(fd, msg_buf, REQUEST_MSG_BUF_LEN - 1, IO_CHECK);
    if(bytes > 0)
    {
        msg_buf[bytes] = '\0';
        request->message += msg_buf;

//printf("msg_buf:\n%s\n", msg_buf);
//printf("\n\n");
//printf("raw [%d]:\n", bytes);
//for(int i = 0; i < bytes; i++) printf("%02X", msg_buf[i]);
//printf("\n\n");

        /* Attempt to Parse Header */
        if(!request->hdr_complete)
        {
            /* Look Through Existing Header Received */
            for(; request->hdr_index < (request->message.getLength() - 4); request->hdr_index++)
            {
                /* Look For \r\n\r\n Separation */ 
                if( (request->message[request->hdr_index + 0] == '\r') && 
                    (request->message[request->hdr_index + 1] == '\n') &&
                    (request->message[request->hdr_index + 2] == '\r') &&
                    (request->message[request->hdr_index + 3] == '\n') )
                {
                    /* Null Terminate Header Portion of Message */
                    request->message.setChar('\0', request->hdr_index);
                    request->hdr_complete = true;
                    request->hdr_index += 4; // moves hdr_index to start of body

                    /* Parse Request */    
                    List<SafeString>* header_list = request->message.split('\r');

                    /* Parse Request Line */
                    try
                    {
                        List<SafeString>* request_line = (*header_list)[0].split(' ');
                        request->verb = EndpointObject::str2verb((*request_line)[0]);
                        request->url = (*request_line)[1].getString(true);
                        delete request_line;
                    }
                    catch(const std::out_of_range& e)
                    {
                        mlog(CRITICAL, "Invalid request line: %s: %s\n", (*header_list)[0].getString(), e.what());
                    }

                    /* Parse Headers */
                    for(int h = 1; h < header_list->length(); h++)
                    {
                        /* Create Key/Value Pairs */
                        List<SafeString>* keyvalue_list = (*header_list)[h].split(':');
                        try
                        {
                            const char* key = (*keyvalue_list)[0].getString();
                            request->headers.add(key, (*keyvalue_list)[1], true);
                        }
                        catch(const std::out_of_range& e)
                        {
                            mlog(CRITICAL, "Invalid header in http request: %s: %s\n", (*header_list)[h].getString(), e.what());
                        }
                        delete keyvalue_list;                
                    }

                    /* Clean Up Header List */
                    delete header_list;

                    /* Get Content Length */
                    try
                    {
                        if(!StringLib::str2long(request->headers["Content-Length"].getString(), &request->content_length))
                        {
                            mlog(CRITICAL, "Invalid Content-Length header: %s\n", request->headers["Content-Length"].getString());
                            status = INVALID_RC; // will close socket
                        }
                    }
                    catch(const std::out_of_range& e)
                    {
                        mlog(CRITICAL, "Http request must supply Content-Length header: %s\n", e.what());
                        status = INVALID_RC; // will close socket
                    }                    
                }
            }
        }

        /* Check If Body Complete */
        if(request->content_length > 0)
        {
            if(request->message.getLength() == request->content_length)
            {
                /* Get Message Body */
                const char* raw_message = request->message.getString();
                request->body = &raw_message[request->hdr_index];

                /* Get Endpoint and New URL */
                const char* endpoint = NULL;
                const char* new_url = NULL;
                extract(request->url, &endpoint, &new_url);
                if(endpoint && new_url)
                {
                    try
                    {
                        /* Get Attached Endpoint Object --> Handle Request */
                        EndpointObject* endpoint_obj = routeTable[endpoint];
                        endpoint_obj->handleRequest(request->id, new_url, request->verb, request->headers, request->body, endpoint_obj);
                    }
                    catch(const std::out_of_range& e)
                    {
                        mlog(CRITICAL, "No attached endpoint at %s: %s\n", endpoint, e.what());
                        status = INVALID_RC; // will close socket
                    }                    
                }
                if(endpoint) delete [] endpoint;
                if(new_url) delete [] new_url;
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
    request_t* request = requests[fd];

    /* Send Data */
    if(request->rsp_buffer_left < request->rsp_buffer_index)
    {
        int bytes_left = request->rsp_buffer_index - request->rsp_buffer_left;
        int bytes = SockLib::socksend(fd, &request->rsp_buffer[request->rsp_buffer_left], bytes_left, IO_CHECK);
        if(bytes > 0)
        {
            request->rsp_buffer_left += bytes;
        }
        else
        {
            // Failed to send data on socket that was marked for writing;
            // therefore return failure which will close socket
            status = INVALID_RC;
        }
    }

    /* Check if Done */
    if(request->rsp_buffer_left == request->rsp_buffer_index)
    {
        request->rsp_buffer_left = 0;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onAlive
 *
 *  Notes: Performed for every existing request
 *----------------------------------------------------------------------------*/
int HttpServer::onAlive(int fd)
{
    request_t* request = requests[fd];

    /* While Room in Buffer */
    while(request->rsp_buffer_index < request->rsp_buffer_size)
    {
        /* Check If Payload Reference Has More Data */
        if(request->ref_data_left > 0)
        {
            /* Populate Buffer */
            int bytes_left = request->rsp_buffer_size - request->rsp_buffer_index;
            int cpylen = MIN(request->ref_data_left, bytes_left);
            int ref_index = request->ref.size - request->ref_data_left;
            uint8_t* ref_buffer = (uint8_t*)request->ref.data;
            LocalLib::copy(&request->rsp_buffer[request->rsp_buffer_index], &ref_buffer[ref_index], cpylen);
            request->rsp_buffer_index += cpylen;
            request->ref_data_left -= cpylen;
        }

        /* Check Need for More Payloads */
        if((request->ref_data_left == 0) && (request->rsp_buffer_index < request->rsp_buffer_size))
        {
            int status = request->rspq->receiveRef(request->ref, IO_CHECK);
            if(status > 0)
            {
                /* Populate Rest of Buffer */
                int bytes_left = request->rsp_buffer_size - request->rsp_buffer_index;
                int cpylen = MIN(request->ref.size, bytes_left);
                LocalLib::copy(&request->rsp_buffer[request->rsp_buffer_index], request->ref.data, cpylen);
                request->rsp_buffer_index += cpylen;

                /* Calculate Payload Left and Dereference if Payload Fully Buffered */
                request->ref_data_left = request->ref.size - cpylen;
                if(request->ref_data_left == 0)
                {
                    request->rspq->dereference(request->ref);
                }

                /* Mark Ready to Write */
                dataToWrite = true;
            }
            else
            {        
                /* No More Data to Write */        
                break;
            }
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * onConnect
 *
 *  Notes: performed on new requests when the request is made
 *----------------------------------------------------------------------------*/
int HttpServer::onConnect(int fd)
{
    int status = 0;

    request_t* request = new request_t;
    LocalLib::set(request, 0, sizeof(request_t));

    if(requests.add(fd, request, true))
    {
        request->id = getUniqueId();

        request->rspq = new Subscriber(request->id);

        request->rsp_buffer_size = LocalLib::getIOMaxsize();
        request->rsp_buffer = new uint8_t[request->rsp_buffer_size];
    }
    else
    {
        mlog(CRITICAL, "HTTP server at %s failed to register request due to duplicate entry\n", request->id);
        status = INVALID_RC;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onDisconnect
 *
 *  Notes: performed on disconnected requests
 *----------------------------------------------------------------------------*/
int HttpServer::onDisconnect(int fd)
{
    int status = 0;

    request_t* request = requests[fd];
    if(requests.remove(fd))
    {
        delete request->id; 
        delete request->rspq;
        delete [] request->rsp_buffer;
        delete request;
    }
    else
    {
        mlog(CRITICAL, "HTTP server at %s failed to release request\n", request->id);
        status = -1;
    }

    return status;
}

