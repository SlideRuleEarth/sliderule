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

#include <math.h>
#include <float.h>
#include <stdarg.h>

#include "core.h"
#include "h5.h"
#include "icesat2.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl06Proxy::OBJECT_TYPE = "Atl06Proxy";
const char* Atl06Proxy::LuaMetaName = "Atl06Proxy";
const struct luaL_Reg Atl06Proxy::LuaMetaTable[] = {
    {NULL,          NULL}
};

Publisher*  Atl06Proxy::rqstPub;
Subscriber* Atl06Proxy::rqstSub;
bool        Atl06Proxy::proxyActive;
Thread**    Atl06Proxy::proxyPids;
Mutex       Atl06Proxy::proxyMut;
int         Atl06Proxy::threadPoolSize;

/******************************************************************************
 * ATL06 PROXY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Atl06Proxy::init (void)
{
    rqstPub = NULL;
    proxyActive = false;
    rqstSub = NULL;
    threadPoolSize = 0;
    proxyPids = NULL;
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void Atl06Proxy::deinit (void)
{
    if(proxyActive)
    {
        proxyActive = false;
        for(int t = 0; t < threadPoolSize; t++)
        {
            delete proxyPids[t];
        }
        if(proxyPids) delete [] proxyPids;
        if(rqstSub) delete rqstSub;
    }

    if(rqstPub) delete rqstPub;
}

/*----------------------------------------------------------------------------
 * luaInit - init(<num_threads>)
 *----------------------------------------------------------------------------*/
int Atl06Proxy::luaInit (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Number of Threads */
        long num_threads = getLuaInteger(L, 1, true, LocalLib::nproc() * CPU_LOAD_FACTOR);

        /* Get Depth of Request Queue */
        long rqst_queue_depth = getLuaInteger(L, 2, true, MsgQ::CFG_DEPTH_STANDARD);

        /* Create Proxy Thread Pool */
        proxyMut.lock();
        {
            if(!proxyActive)
            {
                if(num_threads > 0)
                {
                    rqstPub = new Publisher(NULL, NULL, rqst_queue_depth);
                    proxyActive = true;
                    rqstSub = new Subscriber(*rqstPub);
                    threadPoolSize = num_threads;
                    proxyPids = new Thread* [threadPoolSize];
                    for(int t = 0; t < threadPoolSize; t++)
                    {
                        proxyPids[t] = new Thread(proxyThread, NULL);
                    }
                }
                else
                {
                    proxyMut.unlock();
                    throw RunTimeException(CRITICAL, RTE_ERROR, "Number of threads must be greater than zero");
                }
            }
            else
            {
                proxyMut.unlock();
                throw RunTimeException(CRITICAL, RTE_ERROR, "Atl06Proxy has already been initialized");
            }
        }
        proxyMut.unlock();

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error initializing Atl06Proxy module: %s", e.what());
    }

    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaCreate - create(<resources>, <parameter string>, <outq_name>, <orchestrator_url>)
 *----------------------------------------------------------------------------*/
int Atl06Proxy::luaCreate (lua_State* L)
{
    const char** _resources = NULL;
    int _num_resources = 0;

    try
    {
        /* Check Resource Table Parameter */
        int resources_parm_index = 1;
        if(!lua_istable(L, resources_parm_index))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "must supply table for parameter #1");
        }

        /* Get List of Resources */
        _num_resources = lua_rawlen(L, resources_parm_index);
        if(_num_resources > 0)
        {
            /* Allocate Resource Array */
            _resources = new const char* [_num_resources];
            LocalLib::set(_resources, 0, _num_resources * sizeof(const char*));

            /* Build Resource Array */
            for(int i = 0; i < _num_resources; i++)
            {
                lua_rawgeti(L, resources_parm_index, i+1);
                const char* _resource = getLuaString(L, -1);
                _resources[i] = _resource; // NOT allocated... must be used before constructor returns
                lua_pop(L, 1);
            }
        }

        /* Get Request Parameters */
        const char* _parameters = getLuaString(L, 2);

        /* Get Output Queue */
        const char* outq_name = getLuaString(L, 3);

        /* Get Orhcestrator URL */
        const char* orchestrator_url = getLuaString(L, 4);

        /* Return Reader Object */
        return createLuaObject(L, new Atl06Proxy(L, _resources, _num_resources, _parameters, outq_name, orchestrator_url));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Atl06Proxy: %s", e.what());

        for(int i = 0; i < _num_resources; i++)
        {
            delete [] _resources[i];
        }
        delete [] _resources;

        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl06Proxy::Atl06Proxy (lua_State* L, const char** _resources, int _num_resources, const char* _parameters, const char* _outq_name, const char* _orchestrator_url):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_resources);
    assert(_parameters);
    assert(_outq_name);

    numRequests = _num_resources;
    requests = new atl06_rqst_t[numRequests];
    parameters = StringLib::duplicate(_parameters, MAX_REQUEST_PARAMETER_SIZE);
    orchestratorURL = StringLib::duplicate(_orchestrator_url);

//            HttpClient orchestrator(NULL, rqst->proxy->orchestratorURL);
                /* Get Lock from Orchestrator */
                mlog(INFO, "Processing resource: %s", resource);
                SafeString orch_rqst_data("{'service':'test', 'nodesNeeded': 1, 'timeout': %d}", NODE_LOCK_TIMEOUT);
//                HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::GET, "/discovery/lock", orch_rqst_data.getString(), false, NULL);
//                print2term("%s\n", rsps.response);

    /* Create Publisher */
    outQ = new Publisher(_outq_name);

    /* Populate Requests */
    for(int i = 0; i < numRequests; i++)
    {
        /* Allocate and Initialize Request */
        requests[i].proxy = this;
        requests[i].resource = StringLib::duplicate(_resources[i]);
        requests[i].index = i;
        requests[i].valid = true;
        requests[i].complete = false;

        /* Post Request */
        if(rqstPub->postRef(&requests[i], sizeof(atl06_rqst_t), IO_CHECK) <= 0)
        {
            LuaEndpoint::generateExceptionStatus(RTE_ERROR, outQ, NULL, "Failed to proxy request for %s", requests[i].resource);
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl06Proxy::~Atl06Proxy (void)
{
    for(int i = 0; i < numRequests; i++)
    {
        requests[i].sync.lock();
        {
            if(!requests[i].complete)
            {
                if(requests[i].sync.wait(0, NODE_LOCK_TIMEOUT * 1000))
                {
                    delete [] requests[i].resource;
                }
                else
                {
                    mlog(CRITICAL, "Memory leak due to unfinished proxied request: %s", requests[i].resource);
                }
            }
        }
        requests[i].sync.unlock();

    }
    delete [] requests;
    delete [] parameters;
    delete outQ;
    delete [] orchestratorURL;
}

/*----------------------------------------------------------------------------
 * proxyThread
 *----------------------------------------------------------------------------*/
void* Atl06Proxy::proxyThread (void* parm)
{
    (void)parm;

    while(proxyActive)
    {
        Subscriber::msgRef_t ref;
        int recv_status = rqstSub->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            atl06_rqst_t* rqst = (atl06_rqst_t*)ref.data;
            Atl06Proxy* proxy = rqst->proxy;
//            HttpClient orchestrator(NULL, rqst->proxy->orchestratorURL);
            const char* resource = rqst->resource;
            try
            {
                /* Get Lock from Orchestrator */
                mlog(INFO, "Processing resource: %s", resource);
                SafeString orch_rqst_data("{'service':'test', 'nodesNeeded': 1, 'timeout': %d}", NODE_LOCK_TIMEOUT);
//                HttpClient::rsps_t rsps = orchestrator.request(EndpointObject::GET, "/discovery/lock", orch_rqst_data.getString(), false, NULL);
//                print2term("%s\n", rsps.response);

                // pass request to node

                // stream response back to queue

                /* Mark Complete */
                rqst->complete = true;
                rqst->sync.signal();
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Failure processing request: %s", e.what());
            }
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Failed to receive request: %d", recv_status);
            break;
        }
    }

    return NULL;
}
