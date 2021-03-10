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

#include "OsApi.h"
#include "TraceLib.h"

#ifdef __lttng_tracing__
#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "lttng-core.h"
#endif

#include <cstdarg>
#include <atomic>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

std::atomic<uint32_t> TraceLib::unique_id{1};
Thread::key_t TraceLib::trace_key;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void TraceLib::init (void)
{
    trace_key = Thread::createGlobal();
    Thread::setGlobal(trace_key, (void*)ORIGIN);
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void TraceLib::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * startTrace
 *----------------------------------------------------------------------------*/
uint32_t TraceLib::startTrace(uint32_t parent, const char* name, const char* attributes)
{
    long tid = Thread::getId();

    #ifndef __lttng_tracing__
    (void)parent;
    (void)name;
    (void)attributes;
    (void)tid;
    #endif

    uint32_t id = unique_id++;

    tracepoint(sliderule, start, tid, id, parent, name, attributes);
    return id;
}

/*----------------------------------------------------------------------------
 * startTraceExt
 *----------------------------------------------------------------------------*/
uint32_t TraceLib::startTraceExt(uint32_t parent, const char* name, const char* fmt, ...)
{
    #ifndef __lttng_tracing__
    (void)parent;
    (void)name;
    #endif

    /* Build Formatted Attribute String */
    char attributes[MAX_ATTR_SIZE];
    va_list args;
    va_start(args, fmt);
    int vlen = vsnprintf(attributes, MAX_ATTR_SIZE - 1, fmt, args);
    int msglen = MIN(vlen, MAX_ATTR_SIZE - 1);
    va_end(args);
    if (msglen < 0) msglen = 0;
    attributes[msglen] = '\0';

    /* Execute Trace */
    return startTrace(parent, name, attributes);
}

/*----------------------------------------------------------------------------
 * stopTrace
 *----------------------------------------------------------------------------*/
void TraceLib::stopTrace(uint32_t id)
{
    #ifndef __lttng_tracing__
    (void)id;
    #endif

    tracepoint(sliderule, stop, id);
}

/*----------------------------------------------------------------------------
 * stashId
 *----------------------------------------------------------------------------*/
void TraceLib::stashId (uint32_t id)
{
    Thread::setGlobal(trace_key, (void*)(unsigned long long)id);
}

/*----------------------------------------------------------------------------
 * grabId
 *----------------------------------------------------------------------------*/
uint32_t TraceLib::grabId (void)
{
    return (uint32_t)(unsigned long long)Thread::getGlobal(trace_key);
}
