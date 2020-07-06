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

#ifndef __tracelib__
#define __tracelib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include <atomic>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define ORIGIN  0

#ifdef __lttng_tracing__

#define start_trace(parent, name, attributes) TraceLib::startTrace(parent, name, attributes)
#define start_trace_ext(parent, name, fmt, ...) TraceLib::startTraceExt(parent, name, fmt, __VA_ARGS__)
#define stop_trace(id) TraceLib::stopTrace(id)

#else /* no __lttng_tracing__ */

#define start_trace() {ORIGIN}
#define start_trace_ext() {ORIGIN}
#define stop_trace() {}

#define tracepoint(...) {}

#endif

/******************************************************************************
 * LOG LIBRARY CLASS
 ******************************************************************************/

class TraceLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_ATTR_SIZE = 128;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static uint32_t startTrace      (uint32_t parent, const char* name, const char* attributes);
        static uint32_t startTraceExt   (uint32_t parent, const char* name, const char* fmt, ...) VARG_CHECK(printf, 3, 4);
        static void     stopTrace       (uint32_t id);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static std::atomic<uint32_t> unique_id;
};

#endif  /* __tracelib__ */
