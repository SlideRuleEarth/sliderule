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

    clientMut.lock();
    {
        try
        {
            /* Check Connections */
            if(clients.length() > 0)
            {
                throw LuaExcetion("%d proxy connections active", clients.length())
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
                    int port = getLuaInteger(L, -1));
                    client_info_t* info = new client_info_t;
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
                clientThreadPool[i].add(pid);
            }
        }
        catch(const LuaException& e)
        {
            mlog(CRITICAL, "Error connecting to proxies: %s\n", e.errmsg);
            status = false;
        }
    }
    clientMut.unlock();

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaDisconnect - disconnect()
 *----------------------------------------------------------------------------*/
int H5Proxy::luaDisconnect(lua_State* L)
{
    bool status = true;

    clientMut.lock();
    {
        clientActive = false;
        for(int i = 0; i < clientThreadPool.length(); i++)
        {
            delete clientThreadPool[i];            
        }
        clientThreadPool.clear();
        delete clientRequestQ;
    }
    clientMut.unlock();

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
 * clientThread
 *----------------------------------------------------------------------------*/
void* H5Proxy::clientThread(void* parm)
{
    client_info_t* info = (client_info_t*)parm;
    TcpSocket* sock = new TcpSocket(L, "127.0.0.1", info->port, false, NULL, false);
    Subscriber* sub = new Subscriber(REQUEST_QUEUE);

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
        int bytes_read = sock->readBuffer(buf, bytes_to_read);
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
                    int id_bytes_sent = sock->writeBuffer(&request->id, sizeof(request->id));
                    if(id_bytes_sent != sizeof(request->id))
                    {
                        mlog(CRITICAL, "Failed to send ID in response: %d\n", id_bytes_sent);
                        break;
                    }

                    /* Return Size of Response */
                    int size_bytes_sent = sock->writeBuffer(&info.datasize, sizeof(info.datasize)); 
                    if(size_bytes_sent != sizeof(info.datasize))
                    {
                        mlog(CRITICAL, "Failed to send size in response: %d\n", size_bytes_sent);
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
