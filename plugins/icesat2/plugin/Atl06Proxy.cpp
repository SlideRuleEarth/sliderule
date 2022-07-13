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

/******************************************************************************
 * ATL06 PROXY CLASS
 ******************************************************************************/

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
        if(num_resources > 0)
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

        /* Get Number of Threads */
        long num_threads = getLuaInteger(L, 4, true, LocalLib::nproc() * CPU_LOAD_FACTOR);

        /* Return Reader Object */
        return createLuaObject(L, new Atl06Proxy(L, _resources, _num_resources, _parameters, outq_name, num_threads));
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
Atl06Proxy::Atl06Proxy (lua_State* L, const char** _resources, int _num_resources, const char* _parameters, const char* outq_name, int _num_threads):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_resources);
    assert(_parameters);
    assert(outq_name);
    assert(_num_threads > 0);

    resources = _resources;
    numResources = _num_resources;
    parameters = StringLib::duplicate(_parameters, MAX_REQUEST_PARAMETER_SIZE);

    /* Create Publisher */
    outQ = new Publisher(outq_name);

    /* Initialize Threads */
    active = true;
    threadCount = _num_threads;
    numComplete = 0;
    proxyPids = new Thread* [threadCount];
    LocalLib::set(proxyPids, 0, sizeof(threadCount * sizeof(Thread*)));

    /* Create Proxy Threads */
    for(int t = 0; t < threadCount; t++)
    {
        info_t* info = new info_t;
        info->proxy = this;
        proxyPids[t] = new Thread(proxyThread, info);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Atl06Proxy::~Atl06Proxy (void)
{
    active = false;

    for(int t = 0; t < NUM_TRACKS; t++)
    {
        if(proxyPids[t]) delete proxyPids[t];
    }
    delete [] proxyPids;

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
    /* Get Thread Info */
    info_t* info = (info_t*)parm;
    Atl06Proxy* proxy = info->proxy;

    /* Start Trace */
    uint32_t trace_id = start_trace(INFO, reader->traceId, "atl06_proxy", "{\"resource\":\"%s\"}", info->reader->asset->getName(), info->reader->resource, info->track);
    EventLib::stashId (trace_id); // set thread specific trace id for H5Coro

    try
    {
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Failure during processing of resource %s track %d: %s", info->reader->resource, info->track, e.what());
        reader->generateExceptionStatus(e.code(), e.what());
    }

    /* Handle Global Reader Updates */
    proxy->threadMut.lock();
    {
        /* Count Completion */
        proxy->numComplete++;
        if(reader->numComplete == proxy->threadCount)
        {
            mlog(INFO, "Completed processing resource %s", info->proxy->resource);

            /* Indicate End of Data */
            proxy->signalComplete();
        }
    }
    proxy->threadMut.unlock();

    /* Clean Up Info */
    delete info;

    /* Stop Trace */
    stop_trace(INFO, trace_id);

    /* Return */
    return NULL;
}
