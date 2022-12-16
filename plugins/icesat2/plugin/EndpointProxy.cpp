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
 * luaCreate - create(<endpoint>, <asset>, <resources>, <parameter string>, <timeout>, <outq_name>, <terminator>)
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
        int         _timeout_secs       = getLuaInteger(L, 5, true, RqstParms::DEFAULT_RQST_TIMEOUT); // get timeout in seconds
        const char* _outq_name          = getLuaString(L, 6); // get output queue
        bool        _send_terminator    = getLuaBoolean(L, 7, true, false); // get send terminator flag
        long        _num_threads        = getLuaInteger(L, 8, true, LocalLib::nproc() * CPU_LOAD_FACTOR); // get number of proxy threads
        long        _rqst_queue_depth   = getLuaInteger(L, 9, true, DEFAULT_PROXY_QUEUE_DEPTH); // get depth of request queue for proxy threads

        /* Check Parameters */
        if(_num_threads <= 0) throw RunTimeException(CRITICAL, RTE_ERROR, "Number of threads must be greater than zero");
        else if (_num_threads > MAX_PROXY_THREADS) throw RunTimeException(CRITICAL, RTE_ERROR, "Number of threads must be less than %d", MAX_PROXY_THREADS);

        /* Return Endpoint Proxy Object */
        EndpointProxy* ep = new EndpointProxy(L, _endpoint, _asset, _resources, _num_resources, _parameters, _timeout_secs, _outq_name, _send_terminator, _num_threads, _rqst_queue_depth);
        int retcnt = createLuaObject(L, ep);
        if(_resources) delete [] _resources;
        return retcnt;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating EndpointProxy: %s", e.what());

        for(int i = 0; i < _num_resources; i++)
        {
            delete [] _resources[i];
        }
        if (_resources) delete [] _resources;

        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
EndpointProxy::EndpointProxy (lua_State* L, const char* _endpoint, const char* _asset, const char** _resources, int _num_resources,
                              const char* _parameters, int _timeout_secs, const char* _outq_name, bool _send_terminator,
                              int _num_threads, int _rqst_queue_depth):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_asset);
    assert(_resources);
    assert(_parameters);
    assert(_outq_name);

    numResources = _num_resources;
    timeout = _timeout_secs;
    numProxyThreads = _num_threads;
    rqstQDepth = _rqst_queue_depth;
    sendTerminator = _send_terminator;

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
    parameters  = StringLib::duplicate(_parameters, MAX_REQUEST_PARAMETER_SIZE);
    outQ        = new Publisher(_outq_name, Publisher::defaultFree, numProxyThreads);

    /* Populate Resources Array */
    resources = new const char* [numResources];
    for(int i = 0; i < numResources; i++)
    {
        resources[i] = StringLib::duplicate(_resources[i]);
    }

    /* Initialize Nodes Array */
    nodes = new OrchestratorLib::Node* [numResources];
    LocalLib::set(nodes, 0, sizeof(OrchestratorLib::Node*)); // set all to NULL

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
    delete [] asset;
    delete [] parameters;
}

/*----------------------------------------------------------------------------
 * collatorThread
 *----------------------------------------------------------------------------*/
void* EndpointProxy::collatorThread (void* parm)
{
    EndpointProxy* proxy = (EndpointProxy*)parm;
    int current_resource = 0;

    while(proxy->active && (proxy->outQ->getSubCnt() > 0) && (current_resource < proxy->numResources))
    {
        /* Get Available Nodes */
        int resources_to_process = proxy->numResources - current_resource;
        int num_nodes_to_request = MIN(resources_to_process, proxy->numProxyThreads);
        OrchestratorLib::NodeList* nodes = OrchestratorLib::lock(SERVICE, num_nodes_to_request, proxy->timeout);
        if(nodes)
        {
            for(int i = 0; i < nodes->length(); i++)
            {
                /* Populate Request */
                proxy->nodes[current_resource] = nodes->get(i);

                /* Post Request to Proxy Threads */
                int status = MsgQ::STATE_TIMEOUT;
                while(proxy->active && (status == MsgQ::STATE_TIMEOUT))
                {
                    status = proxy->rqstPub->postCopy(&current_resource, sizeof(current_resource), SYS_TIMEOUT);
                    if(status < 0)
                    {
                        LuaEndpoint::generateExceptionStatus(RTE_ERROR, ERROR, proxy->outQ, NULL, "Failed (%d) to post request for %s", status, proxy->resources[current_resource]);
                        break;
                    }
                }

                /* Bump Current Resource */
                current_resource++;
            }

            /*  If No Nodes Available */
            if(nodes->length() <= 0)
            {
                LocalLib::sleep(0.20); // 5Hz
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

    /* Send Terminator */
    if(proxy->sendTerminator)
    {
        proxy->outQ->postCopy("", 0);
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
        /* Receive Request */
        int current_resource;
        int recv_status = proxy->rqstSub->receiveCopy(&current_resource, sizeof(current_resource), SYS_TIMEOUT);
        if(recv_status > 0)
        {
            /* Get Resource and Node */
            const char* resource = proxy->resources[current_resource];
            OrchestratorLib::Node* node = proxy->nodes[current_resource];
            bool valid = false; // set to true on success

            /* Make Request */
            if(proxy->outQ->getSubCnt() > 0)
            {
                try
                {
                    SafeString path("/source/%s", proxy->endpoint);
                    SafeString data("{\"atl03-asset\": \"%s\", \"resource\": \"%s\", \"parms\": %s, \"timeout\": %d}", proxy->asset, resource, proxy->parameters, proxy->timeout);
                    HttpClient client(NULL, node->member);
                    HttpClient::rsps_t rsps = client.request(EndpointObject::POST, path.getString(), data.getString(), false, proxy->outQ, proxy->timeout * 1000);
                    if(rsps.code == EndpointObject::OK) valid = true;
                    else throw RunTimeException(CRITICAL, RTE_ERROR, "Error code returned from request to %s: %d", node->member, (int)rsps.code);
                }
                catch(const RunTimeException& e)
                {
                    mlog(e.level(), "Failure processing request: %s", e.what());
                }
            }

            /* Unlock Node */
            OrchestratorLib::unlock(&node->transaction, 1);

            /* Post Status */
            int code = valid ? RTE_INFO : RTE_ERROR;
            event_level_t level = valid ? INFO : ERROR;
            LuaEndpoint::generateExceptionStatus(code, level, proxy->outQ, NULL, "%s processing resource [%d out of %d]: %s",
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
