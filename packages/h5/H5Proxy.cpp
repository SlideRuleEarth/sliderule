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

#include "H5Proxy.h"
#include "H5Lib.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* H5Proxy::OBJECT_TYPE = "H5Proxy";
const char* H5Proxy::LuaMetaName = "H5Proxy";
const struct luaL_Reg H5Proxy::LuaMetaTable[] = {
    {NULL,          NULL}
};

const char* H5Proxy::recType = "h5.rqst";
const RecordObject::fieldDef_t H5Proxy::recDef[] = {
    {"id",      RecordObject::UINT32,   offsetof(request_t, id),            1,                  NULL, NATIVE_FLAGS},
    {"op",      RecordObject::UINT32,   offsetof(request_t, operation),     1,                  NULL, NATIVE_FLAGS},
    {"url",     RecordObject::DOUBLE,   offsetof(request_t, url),           MAX_RQST_STR_SIZE,  NULL, NATIVE_FLAGS},
    {"dataset", RecordObject::DOUBLE,   offsetof(request_t, datasetname),   MAX_RQST_STR_SIZE,  NULL, NATIVE_FLAGS},
    {"type",    RecordObject::UINT32,   offsetof(request_t, valtype),       1,                  NULL, NATIVE_FLAGS},
    {"col",     RecordObject::INT64,    offsetof(request_t, col),           1,                  NULL, NATIVE_FLAGS},
    {"start",   RecordObject::INT64,    offsetof(request_t, startrow),      1,                  NULL, NATIVE_FLAGS},
    {"num",     RecordObject::INT64,    offsetof(request_t, numrows),       1,                  NULL, NATIVE_FLAGS}
};

const char* H5Proxy::REQUEST_QUEUE = "h5proxyq";

Cond H5Proxy::clientSignal;
bool H5Proxy::clientActive = false;
List<Thread*> H5Proxy::clientThreadPool;
Publisher* H5Proxy::clientRequestQ = NULL;
Table<H5Proxy::pending_t*, H5Proxy::request_id_t> H5Proxy::clientPending;
uint32_t H5Proxy::clientId = 0;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - proxy(<ip_addr>, <port>)
 *----------------------------------------------------------------------------*/
int H5Proxy::luaCreate (lua_State* L)
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
        return createLuaObject(L, new H5Proxy(L, ip_addr, port));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating H5Proxy: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaConnect - connect([<proxy port1>, <proxy port 2>, ...])
 *----------------------------------------------------------------------------*/
int H5Proxy::luaConnect(lua_State* L)
{
    bool status = true;

    clientSignal.lock();
    {
        try
        {
            /* Check Connections */
            if(clientThreadPool.length() > 0)
            {
                throw LuaException("%d proxy connections active", clientThreadPool.length());
            }

            /* Get List of Proxy Ports to Connect To */
            List<client_info_t*> proxies;
            int tblindex = 1;
            if(lua_type(L, tblindex) == LUA_TTABLE)
            {
                int size = lua_rawlen(L, tblindex);
                for(int e = 0; e < size; e++)
                {
                    lua_rawgeti(L, tblindex, e + 1);
                    int port = getLuaInteger(L, -1);
                    client_info_t* info = new client_info_t;
                    info->L = L;
                    info->port = port;
                    proxies.add(info);
                    lua_pop(L, 1);
                }
            }

            /* Create Request Queue */
            clientRequestQ = new Publisher(REQUEST_QUEUE);

            /* Create Proxy Connection Threads */
            clientActive = true;
            for(int i = 0; i < proxies.length(); i++)
            {
                Thread* pid = new Thread(clientThread, proxies[i]);
                clientThreadPool.add(pid);
            }
        }
        catch(const LuaException& e)
        {
            mlog(CRITICAL, "Error connecting to proxies: %s\n", e.errmsg);
            status = false;
        }
    }
    clientSignal.unlock();

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaDisconnect - disconnect()
 *----------------------------------------------------------------------------*/
int H5Proxy::luaDisconnect(lua_State* L)
{
    bool status = true;

    clientSignal.lock();
    {
        clientActive = false;
        for(int i = 0; i < clientThreadPool.length(); i++)
        {
            delete clientThreadPool[i];            
        }
        clientThreadPool.clear();
        delete clientRequestQ;
        clientRequestQ = NULL;
    }
    clientSignal.unlock();

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void H5Proxy::init (void)
{
    RecordObject::recordDefErr_t rc = RecordObject::defineRecord(recType, "op", sizeof(request_t), recDef, sizeof(recDef) / sizeof(RecordObject::fieldDef_t), 16);
    if(rc != RecordObject::SUCCESS_DEF)
    {
        mlog(CRITICAL, "Failed to define %s: %d\n", recType, rc);
    }
}

/*----------------------------------------------------------------------------
 * read
 * 
 *  Note: caller is responsible for deallocating returned pending data
 *----------------------------------------------------------------------------*/
H5Proxy::pending_t* H5Proxy::read (const char* url, const char* datasetname, RecordObject::valType_t valtype, long col, long startrow, long numrows)
{
    /* Allocate/Initialize Response Structure */
    pending_t* pending = new pending_t;
    pending->request = new RecordObject(recType);
    pending->response = new H5Lib::info_t;
    pending->complete = false;

    /* Initialize Response/Request Structure (except for ID) */
    request_t* request = (request_t*)pending->request->getRecordData();
    StringLib::copy(request->url, url, MAX_RQST_STR_SIZE);
    StringLib::copy(request->datasetname, datasetname, MAX_RQST_STR_SIZE);
    request->valtype = valtype;
    request->col = col;
    request->startrow = startrow;
    request->numrows = numrows;

    /* Initialize Response/Response Structure */
    LocalLib::set(pending->response, 0, sizeof(H5Lib::info_t));

    /* Register and Post */
    clientSignal.lock();
    {
        /* Get Unique ID */
        request->id = clientId++;

        /* Register Response */
        clientPending.add(request->id, pending);

        /* Post Request */
        unsigned char* buffer; // reference to serial buffer
        int size = pending->request->serialize(&buffer, RecordObject::REFERENCE);
        int status = clientRequestQ->postRef(buffer, size, SYS_TIMEOUT);
        if(status != MsgQ::STATE_OKAY)
        {
            mlog(ERROR, "Failed to post request %ud to h5 proxy\n", request->id);

            /* Clean Up Allocations */
            delete pending->response;
            delete pending->request;
            delete pending;
            pending = NULL;
        }
    }
    clientSignal.unlock();

    /* Return Response */
    return pending;
}


/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
bool H5Proxy::join(pending_t* pending, int timeout)
{
    assert(pending);

    clientSignal.lock();
    {
        if(clientActive && !pending->complete)
        {
            clientSignal.wait(0, timeout);
        }
    }
    clientSignal.unlock();

    return pending->complete;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
H5Proxy::H5Proxy(lua_State* L, const char* _ip_addr, int _port):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    ipAddr = StringLib::duplicate(_ip_addr);
    port = _port;

    active = true; 
    pid = new Thread(requestThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
H5Proxy::~H5Proxy(void)
{
    active = false;
    delete pid;
    if(ipAddr) delete [] ipAddr;
}

/*----------------------------------------------------------------------------
 * sockFixedRead
 *----------------------------------------------------------------------------*/
int H5Proxy::sockFixedRead (TcpSocket* sock, unsigned char* buf, int size)
{
    int bytes_to_read = size;    

    while(clientActive && bytes_to_read > 0)
    {
        int offset = size - bytes_to_read;
        int bytes_read = sock->readBuffer(&buf[offset], bytes_to_read);
        if(bytes_read > 0)
        {
            bytes_to_read -= bytes_read;
        }
        else if(bytes_read != TIMEOUT_RC)
        {
            mlog(WARNING, "Failed to read response in proxy client... back to listening\n");
            sock->closeConnection();
            break;
        }
    }

    return size - bytes_to_read;
}

/*----------------------------------------------------------------------------
 * clientThread
 *----------------------------------------------------------------------------*/
void* H5Proxy::clientThread(void* parm)
{
    client_info_t* info = (client_info_t*)parm;

    /* Allocate Socket and Subscriber */
    TcpSocket* sock = new TcpSocket(info->L, "127.0.0.1", info->port, false, NULL, false);
    Subscriber* sub = new Subscriber(REQUEST_QUEUE);

    /* Initialize State Machine */
    bool request_sent = true;
    bool ref_dereferenced = true;
    int status = 0;
    Subscriber::msgRef_t ref;

    /* Read Loop */
    while(clientActive)
    {
        if(sock->isConnected())
        {
            /* Receive Request */
            if(request_sent)
            {
                status = sub->receiveRef(ref, SYS_TIMEOUT);
                request_sent = false;
                ref_dereferenced = false;
            }

            /* Send Request */
            if(status > 0)
            {
                int bytes_sent = sock->writeBuffer(ref.data, ref.size);
                if(bytes_sent > 0)
                {
                    request_sent = true;

                    /* Dereference Message */
                    sub->dereference(ref);
                    ref_dereferenced = true;

                    /* Read Response */
                    do
                    {
                        request_id_t request_id;

                        /* Read Response ID */
                        int request_id_size = sizeof(request_id_t);
                        if(sockFixedRead(sock, (unsigned char*)&request_id, request_id_size) != request_id_size) break;

                        /* Look Up Pending */
                        pending_t* pending;
                        clientSignal.lock();
                        {
                            pending = clientPending[request_id];
                        }
                        clientSignal.unlock();

                        /* Read Info */
                        int info_size = sizeof(H5Lib::info_t);
                        if(sockFixedRead(sock, (unsigned char*)pending->response, info_size) != info_size) break;

                        /* Allocate and Read Data */
                        int data_size = pending->response->datasize;
                        pending->response->data = new unsigned char [data_size];
                        if(sockFixedRead(sock, (unsigned char*)pending->response->data, data_size) != data_size) break;

                        /* Mark Complete */
                        clientSignal.lock();
                        {
                            pending->complete = true;
                            clientPending.remove(request_id);
                            clientSignal.signal();
                        }
                        clientSignal.unlock();
                    } while(false);
                }
                else if(bytes_sent == SHUTDOWN_RC)
                {
                    mlog(WARNING, "Shutting down proxy client for port %d... back to listening\n", info->port);
                    sock->closeConnection();
                }
                else if(bytes_sent != TIMEOUT_RC)
                {
                    mlog(ERROR, "Failed (%d) to send request to proxy on port %d... back to listening\n", bytes_sent, info->port);
                    sock->closeConnection();
                }
            }
        }
        else
        {
            mlog(WARNING, "Proxy client not connected to port %d... sleeping and retrying\n", info->port);
            LocalLib::performIOTimeout();
        }
    }

    /* Clean Up Reference*/
    if(ref_dereferenced == false)
    {
        sub->dereference(ref);
    }

    /* Clean Up Allocations */
    delete sub;
    delete sock;
    delete info;

    /* Exit Thread */
    return NULL;
}

/*----------------------------------------------------------------------------
 * requestThread
 *----------------------------------------------------------------------------*/
void* H5Proxy::requestThread(void* parm)
{
    /* Get Proxy Object */
    H5Proxy* p = (H5Proxy*)parm;

    /* Get Socket */
    TcpSocket* sock = new TcpSocket(p->LuaState, p->ipAddr, p->port, true, NULL, false);

    /* Get I/O Buffer */
    int recsize = RecordObject::getRecordSize(recType);
    unsigned char* buf = new unsigned char [recsize];
    int bytes_to_read = recsize;

    /* Read Loop */
    while(p->active)
    {
        /* Read Request */
        int offset = recsize - bytes_to_read;
        int bytes_read = sock->readBuffer(&buf[offset], bytes_to_read);
        if(bytes_read > 0)
        {
            /* Check Completeness */
            bytes_to_read -= bytes_read;
            if(bytes_to_read > 0) continue;
            else bytes_to_read = recsize;

            /* Create Record Interface */
            RecordInterface* rec = new RecordInterface(buf, recsize);

            /* Get Request Structure */
            request_t* request = (request_t*)rec->getRecordData();

            /* Call Into H5Lib */
            H5Lib::info_t info = H5Lib::read(request->url, request->datasetname, (RecordObject::valType_t)request->valtype, request->col, request->startrow, request->numrows);
            if(info.datasize > 0)
            {
                /* Write Response */
                do
                {
                    /* Return Response ID */
                    int id_bytes_sent = sock->writeBuffer(&request->id, sizeof(request_id_t));
                    if(id_bytes_sent != sizeof(request->id))
                    {
                        mlog(CRITICAL, "Failed to send ID in response: %d\n", id_bytes_sent);
                        break;
                    }

                    /* Return Info */
                    int info_bytes_sent = sock->writeBuffer(&info, sizeof(info)); 
                    if(info_bytes_sent != sizeof(info))
                    {
                        mlog(CRITICAL, "Failed to send info in response: %d\n", info_bytes_sent);
                        break;
                    }

                    /* Return Data */
                    int data_bytes_sent = sock->writeBuffer(info.data, info.datasize);
                    if(data_bytes_sent != info.datasize)
                    {
                        mlog(CRITICAL, "Failed to send data in response: %d\n", data_bytes_sent);
                        break;
                    }
                } while(false);

                /* Clean Up Data */
                delete [] info.data;
            }

            /* Clean Up Record Interface */
            delete rec;
        }
        else if(bytes_read != TIMEOUT_RC)
        {
            if(bytes_read == SHUTDOWN_RC)
            {
                mlog(CRITICAL, "Shutting down proxy... back to listening\n");
            }
            else
            {
                mlog(ERROR, "Fatal error reading request (%d)... aborting!\n", bytes_read);
                break;
            }                        
        }
    }

    /* Clean Up */
    delete [] buf;
    delete sock;
    p->signalComplete();
    return NULL;
}
