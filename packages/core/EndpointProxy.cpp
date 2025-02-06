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

#include "OsApi.h"
#include "CurlLib.h"
#include "RequestFields.h"
#include "EndpointProxy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* EndpointProxy::SERVICE = "sliderule";

const char* EndpointProxy::OBJECT_TYPE = "EndpointProxy";
const char* EndpointProxy::LUA_META_NAME = "EndpointProxy";
const struct luaL_Reg EndpointProxy::LUA_META_TABLE[] = {
    {"totalresources",      luaTotalResources},
    {"completeresources",   luaCompleteResources},
    {"proxythreads",        luaNumProxyThreads},
    {NULL,                  NULL}
};

/******************************************************************************
 * ATL06 PROXY CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<endpoint>, <asset>, <resources>, <parameter string>, <timeout>, <outq_name>, <terminator>)
 *----------------------------------------------------------------------------*/
int EndpointProxy::luaCreate (lua_State* L)
{
    const char** _resources = NULL;
    int _num_resources = 0;

    try
    {
        /* Get Parameters */
        const char* _endpoint = getLuaString(L, 1); // get endpoint

        /* Check Resource Table Parameter */
        const int resources_parm_index = 2;
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
            memset(_resources, 0, _num_resources * sizeof(const char*));

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
        const char* _parameters         = getLuaString(L, 3); // get request parameters
        const int   _timeout_secs       = getLuaInteger(L, 4, true, EndpointProxy::DEFAULT_TIMEOUT); // get timeout in seconds
        const int   _locks_per_node     = getLuaInteger(L, 5, true, 1); // get the number of locks per node to request
        const char* _outq_name          = getLuaString(L, 6); // get output queue
        const bool  _send_terminator    = getLuaBoolean(L, 7, true, false); // get send terminator flag
        const long  _cluster_size_hint  = getLuaInteger(L, 8, true, 0);

        /* Return Endpoint Proxy Object */
        EndpointProxy* ep = new EndpointProxy(L, _endpoint, _resources, _num_resources, _parameters, _timeout_secs, _locks_per_node, _outq_name, _send_terminator, _cluster_size_hint);
        const int retcnt = createLuaObject(L, ep);
        delete [] _resources;
        return retcnt;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating EndpointProxy: %s", e.what());

        if(_resources) // unnecessary check, but supplied for static analysis
        {
            for(int i = 0; i < _num_resources; i++)
            {
                delete [] _resources[i];
            }
            delete [] _resources;
        }

        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointProxy::EndpointProxy (lua_State* L, const char* _endpoint, const char** _resources, int _num_resources,
                              const char* _parameters, int _timeout_secs, int _locks_per_node, const char* _outq_name,
                              bool _send_terminator, int _cluster_size_hint):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
    assert(_resources);
    assert(_parameters);
    assert(_outq_name);

    numResources = _num_resources;
    timeout = _timeout_secs;
    locksPerNode = _locks_per_node;
    sendTerminator = _send_terminator;

    /*
     * Set Number of Proxy Threads
     *  - if a hint for the size of the cluster is provided, then use it
     *  - otherwise query the orchestrator for the number of registered nodes
     *  - set the number of proxy threads to the maximum number of concurrent requests the cluster can handle
     */
    int num_nodes = 0;
    if(_cluster_size_hint > 0) num_nodes = _cluster_size_hint;
    else num_nodes = OrchestratorLib::getNodes();
    if(num_nodes > 0)
    {
        const int max_possible_concurrent_requests = (OrchestratorLib::MAX_LOCKS_PER_NODE / locksPerNode) * num_nodes;
        const int candidate_num_threads = MIN(max_possible_concurrent_requests, numResources);
        numProxyThreads = MIN(candidate_num_threads, MAX_PROXY_THREADS);
    }
    else
    {
        numProxyThreads = MIN(numResources, DEFAULT_PROXY_THREADS);
    }

    /* Completion Condition */
    numResourcesComplete = 0;

    /* Proxy Active */
    active = true;

    /* Create Proxy Threads */
    rqstPub = new Publisher(NULL, NULL, PROXY_QUEUE_DEPTH);
    rqstSub = new Subscriber(*rqstPub);
    proxyPids = new Thread* [numProxyThreads];
    for(int t = 0; t < numProxyThreads; t++)
    {
        proxyPids[t] = new Thread(proxyThread, this);
    }

    /* Allocate Data Members */
    endpoint    = StringLib::duplicate(_endpoint);
    parameters  = StringLib::duplicate(_parameters);
    outQ        = new Publisher(_outq_name, Publisher::defaultFree, numProxyThreads);

    /* Populate Resources Array */
    resources = new const char* [numResources];
    for(int i = 0; i < numResources; i++)
    {
        resources[i] = StringLib::duplicate(_resources[i]);
    }

    /* Initialize Nodes Array */
    nodes = new OrchestratorLib::Node* [numResources];
    memset(nodes, 0, sizeof(OrchestratorLib::Node*) * numResources); // set all to NULL

    /* Start Collator Thread */
    collatorPid = new Thread(collatorThread, this);
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

    /* Delete Resources */
    for(int i = 0; i < numResources; i++)
    {
        delete [] resources[i];
    }
    delete [] resources;

    /* Delete Nodes */
    for(int i = 0; i < numResources; i++)
    {
        if(nodes[i]) delete nodes[i];
    }
    delete [] nodes;

    /* Delete Allocated Memory */
    delete [] endpoint;
    delete [] parameters;
}

/*----------------------------------------------------------------------------
 * luaTotalResources - totalresources()
 *----------------------------------------------------------------------------*/
int EndpointProxy::luaTotalResources (lua_State* L)
{
    try
    {
        EndpointProxy* lua_obj = dynamic_cast<EndpointProxy*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->numResources);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting total resources: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaCompleteResources - completeresources()
 *----------------------------------------------------------------------------*/
int EndpointProxy::luaCompleteResources (lua_State* L)
{
    try
    {
        EndpointProxy* lua_obj = dynamic_cast<EndpointProxy*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->numResourcesComplete);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting completed resources: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * luaNumProxyThreads - proxythreads()
 *----------------------------------------------------------------------------*/
int EndpointProxy::luaNumProxyThreads (lua_State* L)
{
    try
    {
        EndpointProxy* lua_obj = dynamic_cast<EndpointProxy*>(getLuaSelf(L, 1));
        lua_pushinteger(L, lua_obj->numProxyThreads);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting number of proxy threads: %s", e.what());
        lua_pushnil(L);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * collatorThread
 *----------------------------------------------------------------------------*/
void* EndpointProxy::collatorThread (void* parm)
{
    EndpointProxy* proxy = reinterpret_cast<EndpointProxy*>(parm);
    int current_resource = 0;

    alert(INFO, RTE_INFO, proxy->outQ, NULL, "Starting proxy for %s to process %d resource(s) with %d thread(s)", proxy->endpoint, proxy->numResources, proxy->numProxyThreads);

    while(proxy->active && (proxy->outQ->getSubCnt() > 0) && (current_resource < proxy->numResources))
    {
        /* Get Available Nodes */
        const int resources_to_process = proxy->numResources - current_resource;
        const int num_nodes_to_request = MIN(resources_to_process, proxy->numProxyThreads);
        vector<OrchestratorLib::Node*>* nodes = OrchestratorLib::lock(SERVICE, num_nodes_to_request, proxy->timeout, proxy->locksPerNode);
        if(nodes)
        {
            for(unsigned i = 0; i < nodes->size(); i++)
            {
                /* Sanity Check Current Resources */
                if(current_resource >= proxy->numResources)
                {
                    mlog(CRITICAL, "Inconsistent number of nodes returned from orchestrator: %ld > %d", nodes->size(), num_nodes_to_request);
                    for(unsigned j = i; j < nodes->size(); j++) delete nodes->at(j);
                    break;
                }

                /* Populate Request */
                proxy->nodes[current_resource] = nodes->at(i);

                /* Post Request to Proxy Threads */
                int status = MsgQ::STATE_TIMEOUT;
                while(proxy->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = proxy->rqstPub->postCopy(&current_resource, sizeof(current_resource), SYS_TIMEOUT);
                    if(status < 0)
                    {
                        alert(ERROR, RTE_ERROR, proxy->outQ, NULL, "Failed (%d) to post request for %s", status, proxy->resources[current_resource]);
                        break;
                    }
                }

                /* Bump Current Resource */
                current_resource++;
            }

            /*  If No Nodes Available */
            if(nodes->empty())
            {
                OsApi::performIOTimeout();
            }

            /* Free Node List (individual nodes still remain) */
            delete nodes;
        }
        else
        {
            mlog(CRITICAL, "Unable to reach orchestrator... abandoning proxy request!");
            proxy->active = false;
        }
    }

    /* Check if All Resources Completed */
    proxy->completion.lock();
    {
        while(proxy->active && (proxy->numResourcesComplete < proxy->numResources))
        {
            proxy->completion.wait(0, SYS_TIMEOUT);
        }
    }
    proxy->completion.unlock();

    /* Send Terminator */
    if(proxy->sendTerminator)
    {
        int status = MsgQ::STATE_TIMEOUT;
        while(proxy->active && (status == MsgQ::STATE_TIMEOUT))
        {
            status = proxy->outQ->postCopy("", 0, SYS_TIMEOUT);
            if(status < 0)
            {
                mlog(CRITICAL, "Failed (%d) to post terminator", status);
                break;
            }
        }
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
    EndpointProxy* proxy = reinterpret_cast<EndpointProxy*>(parm);

    while(proxy->active)
    {
        /* Receive Request */
        int current_resource;
        const int recv_status = proxy->rqstSub->receiveCopy(&current_resource, sizeof(current_resource), SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Get Resource and Node */
            const char* resource = proxy->resources[current_resource];
            OrchestratorLib::Node* node = proxy->nodes[current_resource];
            bool valid = false; // set to true on success
            bool need_to_free_node = false; // set to true when retries occur
            long failed_transactions[NUM_RETRIES];
            int num_failed = 0;

            /* Make (possibly multiple) Request(s) */
            int attempts = NUM_RETRIES;
            while(!valid && attempts-- > 0 && node)
            {
                /* Make Request */
                if(proxy->outQ->getSubCnt() > 0)
                {
                    try
                    {
                        const FString url("%s/source/%s", node->member, proxy->endpoint);
                        const FString data("{\"resource\": \"%s\", \"key_space\": %d, \"parms\": %s}", resource, current_resource, proxy->parameters);
                        const long http_code = CurlLib::postAsRecord(url.c_str(), data.c_str(), proxy->outQ, false, proxy->timeout, &proxy->active);
                        if(http_code == EndpointObject::OK) valid = true;
                        else throw RunTimeException(CRITICAL, RTE_ERROR, "Error code returned from request to %s: %d", node->member, (int)http_code);
                    }
                    catch(const RunTimeException& e)
                    {
                        mlog(e.level(), "Failure processing request: %s", e.what());
                    }
                }

                /* Unlock Node (on success) */
                if(valid) OrchestratorLib::unlock(&node->transaction, 1);
                else failed_transactions[num_failed++] = node->transaction;

                /* Reset Node */
                if(need_to_free_node) delete node;
                node = NULL;

                /* Handle Retry on Failure */
                if(!valid && attempts > 0)
                {
                    mlog(CRITICAL, "Retrying processing resource [%d out of %d]: %s", current_resource + 1, proxy->numResources, resource);
                    while(proxy->active && (proxy->outQ->getSubCnt() > 0) && !node)
                    {
                        vector<OrchestratorLib::Node*>* nodes = OrchestratorLib::lock(SERVICE, 1, proxy->timeout, proxy->locksPerNode);
                        if(nodes)
                        {
                            /* Check Number of Nodes Returned */
                            if(!nodes->empty())
                            {
                                node = nodes->at(0);
                                need_to_free_node = true;

                                /* Sanity Check Number of Nodes Returned */
                                if(nodes->size() > 1)
                                {
                                    mlog(CRITICAL, "Inconsistent number of nodes returned from orchestrator: %ld > 1", nodes->size());
                                    for(unsigned j = 1; j < nodes->size(); j++) delete nodes->at(j);
                                }
                            }
                            else // if(nodes->empty())
                            {
                                OsApi::performIOTimeout();
                            }

                            /* Clean Up Nodes */
                            delete nodes;
                        }
                        else
                        {
                            mlog(CRITICAL, "Unable to reach orchestrator... abandoning retries!");
                            attempts = 0; // breaks out of above loop
                            break;
                        }
                    }
                }
            }

            /* Unlock Failed Transactions */
            if(num_failed > 0)
            {
                OrchestratorLib::unlock(failed_transactions, num_failed);
            }

            /* Resource Completed */
            proxy->completion.lock();
            {
                proxy->numResourcesComplete++;
                if(proxy->numResourcesComplete >= proxy->numResources)
                {
                    proxy->completion.signal();
                }
            }
            proxy->completion.unlock();

            /* Post Status */
            const int code = valid ? RTE_INFO : RTE_ERROR;
            const event_level_t level = valid ? INFO : ERROR;
            alert(level, code, proxy->outQ, NULL, "%s processing resource [%d out of %d]: %s",
                                                    valid ? "Successfully completed" : "Failed to complete",
                                                    current_resource + 1, proxy->numResources, resource);
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Failed (%d) to receive request... abandoning proxy request", recv_status);
            break;
        }
    }

    return NULL;
}
