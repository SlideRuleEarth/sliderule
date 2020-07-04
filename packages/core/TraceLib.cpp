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

#include "OsApi.h"
#include "TraceLib.h"

#ifdef LTTNG_TRACING
#include "lltng-core.h"
#endif

#include <cstdarg>
#include <atomic>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

std::atomic<uint32_t> TraceLib::unique_id{0};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * startTrace
 *----------------------------------------------------------------------------*/
inline uint32_t TraceLib::startTrace(uint32_t parent, const char* name, const char* attributes)
{
    uint32_t id = unique_id++;
    tracepoint(sliderule, start, id, parent, name, attributes);
    return id;
}

/*----------------------------------------------------------------------------
 * startTraceExt
 *----------------------------------------------------------------------------*/
uint32_t TraceLib::startTraceExt(uint32_t parent, const char* name, const char* fmt, ...)
{
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
    startTrace(parent, name, attributes);
}

/*----------------------------------------------------------------------------
 * stopTrace
 *----------------------------------------------------------------------------*/
inline void TraceLib::stopTrace(uint32_t id)
{
    tracepoint(sliderule, stop, id);
}
