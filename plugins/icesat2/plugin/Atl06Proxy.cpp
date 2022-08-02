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
 * luaCreate - create(<resources>, <parameter string>, <outq_name>)
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
                const char* _resource = getLuaString(L, 2);
                _resources[i] = StringLib::duplicate(_resource);
                lua_pop(L, 1);
            }
        }

        /* Get Request Parameters */
        const char* _parameters = getLuaString(L, 2);

        /* Get Output Queue */
        const char* outq_name = getLuaString(L, 3);

        /* Return Reader Object */
        return createLuaObject(L, new Atl06Proxy(L, _resources, _num_resources, _parameters, outq_name));
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
Atl06Proxy::Atl06Proxy (lua_State* L, const char** _resources, int _num_resources, const char* _parameters, const char* outq_name):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_resources);
    assert(_parameters);
    assert(outq_name);

    resources = _resources;
    numResources = _num_resources;
    parameters = StringLib::duplicate(_parameters, MAX_REQUEST_PARAMETER_SIZE);

    /* Create Publisher */
    outQ = new Publisher(outq_name);

    /* Populate Requests */
    for(int i = 0; i < numResources; i++)
    {
        atl06_rqst_t atl06_rqst = {
            .proxy = this,
            .resource_index = i
        };
        rqstPub->postCopy(&atl06_rqst, sizeof(atl06_rqst_t), IO_CHECK);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl06Proxy::~Atl06Proxy (void)
{
    for(int i = 0; i < numResources; i++)
    {
        delete [] resources[i];
    }
    delete [] resources;

    delete parameters;

    delete outQ;
}

/*----------------------------------------------------------------------------
 * proxyThread
 *----------------------------------------------------------------------------*/
void* Atl06Proxy::proxyThread (void* parm)
{
    (void)parm;

    while(proxyActive)
    {
        atl06_rqst_t rqst;
        int recv_status = rqstSub->receiveCopy(&rqst, sizeof(atl06_rqst_t), SYS_TIMEOUT);
        if(recv_status > 0)
        {
            Atl06Proxy* proxy = rqst.proxy;
            const char* resource = rqst.proxy->resources[rqst.resource_index];

            try
            {
                mlog(CRITICAL, "Processing resource: %s\n", resource);
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
