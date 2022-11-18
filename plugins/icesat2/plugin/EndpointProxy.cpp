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
#include "netsvc.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EndpointProxy::SERVICE = "sliderule";

const char* EndpointProxy::OBJECT_TYPE = "EndpointProxy";
const char* EndpointProxy::LuaMetaName = "EndpointProxy";
const struct luaL_Reg EndpointProxy::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * ATL06 PROXY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<endpoint>, <asset>, <resources>, <parameter string>, <timeout>, <outq_name>)
 *----------------------------------------------------------------------------*/
int EndpointProxy::luaCreate (lua_State* L)
{
    const char** _resources = NULL;
    int _num_resources = 0;

    try
    {
        /* Get Parameters */
        const char* _endpoint   = getLuaString(L, 1); // get endpoint
        const char* _asset      = getLuaString(L, 2); // get asset

        /* Check Resource Table Parameter */
        int resources_parm_index = 3;
        if(!lua_istable(L, resources_parm_index))
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "must supply table for resource list");
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

        /* Get Parameters Continued */
        const char* _parameters         = getLuaString(L, 4); // get request parameters
        int         _timeout_secs       = getLuaInteger(L, 5, true, PARM_DEFAULT_RQST_TIMEOUT); // get timeout in seconds
        const char* _outq_name          = getLuaString(L, 6); // get output queue
        long        _num_threads        = getLuaInteger(L, 7, true, LocalLib::nproc() * CPU_LOAD_FACTOR); // get number of proxy threads
        long        _rqst_queue_depth   = getLuaInteger(L, 8, true, DEFAULT_PROXY_QUEUE_DEPTH); // get depth of request queue for proxy threads

        /* Check Parameters */
        if(_num_threads <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Number of threads must be greater than zero");
        else if (_num_threads > MAX_PROXY_THREADS) throw RunTimeException(CRITICAL, RTE_ERROR, "Number of threads must be less than %d", MAX_PROXY_THREADS);

        /* Return Reader Object */
        return createLuaObject(L, new EndpointProxy(L, _endpoint, _asset, _resources, _num_resources, _parameters, _timeout_secs, _outq_name, _num_threads, _rqst_queue_depth));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating EndpointProxy: %s", e.what());

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
EndpointProxy::EndpointProxy (lua_State* L, const char* _endpoint, const char* _asset, const char** _resources, int _num_resources,
                              const char* _parameters, int _timeout_secs, const char* _outq_name,
                              int _num_threads, int _rqst_queue_depth):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_asset);
    assert(_resources);
    assert(_parameters);
    assert(_outq_name);

    numRequests = _num_resources;
    timeout = _timeout_secs;
    numProxyThreads = _num_threads;
    rqstQDepth = _rqst_queue_depth;

    /* Proxy Active */
    active = true;

    /* Create Proxy Threads */
    rqstPub = new Publisher(NULL, NULL, rqstQDepth);
    rqstSub = new Subscriber(*rqstPub);
    proxyPids = new Thread* [numProxyThreads];
    for(int t = 0; t < numProxyThreads; t++)
    {
        proxyPids[t] = new Thread(proxyThread, this);
    }

    /* Allocate Data Members */
    endpoint    = StringLib::duplicate(_endpoint);
    asset       = StringLib::duplicate(_asset);
    requests    = new rqst_t[numRequests];
    parameters  = StringLib::duplicate(_parameters, MAX_REQUEST_PARAMETER_SIZE);
    outQ        = new Publisher(_outq_name);

    /* Get First Round of Nodes */
    int num_nodes_to_request = MIN(numRequests, numProxyThreads);
    OrchestratorLib::NodeList* nodes = OrchestratorLib::lock(SERVICE, num_nodes_to_request, timeout);

    /* Populate Requests */
    for(int i = 0; i < numRequests; i++)
    {
        /* Initialize Request */
        requests[i].resource = StringLib::duplicate(_resources[i]);
        requests[i].node = NULL;
        requests[i].valid = false;
        requests[i].complete = false;
        requests[i].terminated = false;

        /* Assign Node if Available */
        if(nodes && i < nodes->length())
        {
            requests[i].node = nodes->get(i);
        }
    }

    /* Free Node List (individual nodes still remain) */
    if(nodes) delete nodes;

    /* Start Collator Thread */
    collatorPid = new Thread(collatorThread, this);

    /* Post Requests to Proxy Threads */
    for(int i = 0; i < numRequests; i++)
    {
        rqst_t* rqst = &requests[i];
        if(rqstPub->postCopy(&rqst, sizeof(rqst), IO_CHECK) <= 0)
        {
            LuaEndpoint::generateExceptionStatus(RTE_ERROR, ERROR, outQ, NULL, "Failed to proxy request for %s", requests[i].resource);
        }
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
EndpointProxy::~EndpointProxy (void)
{
    /* Join and Delete Threads */
    active = false;
    for(int i = 0; i < numProxyThreads; i++)
    {
        delete proxyPids[i];
    }
    delete [] proxyPids;
    delete collatorPid;

    /* Delete Queues */
    delete rqstPub;
    delete rqstSub;
    delete outQ;

    /* Delete Allocated Memory */
    delete [] requests;
    delete [] endpoint;
    delete [] asset;
    delete [] parameters;
}

/*----------------------------------------------------------------------------
 * collatorThread
 *----------------------------------------------------------------------------*/
void* EndpointProxy::collatorThread (void* parm)
{
    EndpointProxy* proxy = (EndpointProxy*)parm;
    int num_terminated = 0;

    while(proxy->active)
    {
        /* Check Completion of All Requests */
        for(int i = 0; i < proxy->numRequests; i++)
        {
            rqst_t* rqst = &proxy->requests[i];
            if(!rqst->terminated)
            {
                rqst->sync.lock();
                {
                    /* Wait for Completion */
                    if(!rqst->complete)
                    {
                        rqst->sync.wait(0, COLLATOR_POLL_RATE);
                    }

                    /* Mark as Terminated */
                    if(rqst->complete)
                    {
                        rqst->terminated = true;
                        num_terminated++;
                    }
                }
                rqst->sync.unlock();

                /* Process Completion */
                if(rqst->terminated)
                {
                    /* Post Status */
                    int code = rqst->valid ? RTE_INFO : RTE_ERROR;
                    event_level_t level = rqst->valid ? INFO : ERROR;
                    LuaEndpoint::generateExceptionStatus(code, level, proxy->outQ, NULL, "%s processing resource [%d out of %d]: %s",
                                                            rqst->valid ? "Successfully completed" : "Failed to complete",
                                                            num_terminated, proxy->numRequests, rqst->resource);

                    /* Clean Up Request */
                    delete [] rqst->resource;
                    delete rqst->node;
                }
            }
        }

        /* Check if Done */
        if(num_terminated == proxy->numRequests) break;
        else LocalLib::performIOTimeout();
    }

    /* Signal Complete */
    proxy->signalComplete();
    return NULL;
}

/*----------------------------------------------------------------------------
 * proxyThread
 *----------------------------------------------------------------------------*/
void* EndpointProxy::proxyThread (void* parm)
{
    EndpointProxy* proxy = (EndpointProxy*)parm;

    while(proxy->active)
    {
        rqst_t* rqst;
        int recv_status = proxy->rqstSub->receiveCopy(&rqst, sizeof(rqst), SYS_TIMEOUT);
        if(recv_status > 0)
        {
            try
            {
                /* Get Lock from Orchestrator */
                double expiration_time = TimeLib::latchtime() + proxy->timeout;
                while(proxy->active && !rqst->node && (expiration_time > TimeLib::latchtime()))
                {
                    OrchestratorLib::NodeList* nodes = OrchestratorLib::lock(SERVICE, 1, proxy->timeout);
                    if(!nodes)                      throw RunTimeException(CRITICAL, RTE_ERROR, "unable to reach orchestrator");
                    else if(nodes->length() > 0)    rqst->node = nodes->get(0); // freed in collator
                    else                            LocalLib::sleep(1);
                    delete nodes;
                }

                /* Proxy Request */
                if(rqst->node)
                {
                    /* Make Request */
                    SafeString path("/source/%s", proxy->endpoint);
                    SafeString data("{\"atl03-asset\": \"%s\", \"resource\": \"%s\", \"parms\": %s, \"timeout\": %d}",
                                    proxy->asset, rqst->resource, proxy->parameters, proxy->timeout);
                    HttpClient client(NULL, rqst->node->member);
                    HttpClient::rsps_t rsps = client.request(EndpointObject::POST, path.getString(), data.getString(), false, proxy->outQ, proxy->timeout * 1000);
                    if(rsps.code == EndpointObject::OK)
                    {
                        rqst->valid = true;
                    }
                    else
                    {
                        mlog(CRITICAL, "Failed to proxy request to %s: %d", rqst->node->member, (int)rsps.code);
                    }
                }
                else
                {
                    /* Timeout Occurred */
                    mlog(CRITICAL, "Timeout processing resource %s - unable to acquire node", rqst->resource);
                }
            }
            catch(const RunTimeException& e)
            {
                mlog(e.level(), "Failure processing request: %s", e.what());
            }

            /* Unlock Node */
            if(rqst->node)
            {
                OrchestratorLib::unlock(&rqst->node->transaction, 1);
            }

            /* Mark Complete */
            rqst->sync.lock();
            {
                rqst->complete = true;
                rqst->sync.signal();
            }
            rqst->sync.unlock();
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Failed to receive request: %d", recv_status);
            break;
        }
    }

    return NULL;
}
