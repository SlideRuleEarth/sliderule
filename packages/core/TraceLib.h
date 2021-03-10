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

#define start_trace(parent,...) {ORIGIN}; (void)parent;
#define start_trace_ext(parent,...) {ORIGIN}; (void)parent;
#define stop_trace(id,...) {(void)id;}

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

        static void         init            (void);
        static void         deinit          (void);

        static uint32_t     startTrace      (uint32_t parent, const char* name, const char* attributes);
        static uint32_t     startTraceExt   (uint32_t parent, const char* name, const char* fmt, ...) VARG_CHECK(printf, 3, 4);
        static void         stopTrace       (uint32_t id);

        static void         stashId         (uint32_t id);
        static uint32_t     grabId          (void);


        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static std::atomic<uint32_t> unique_id;
        static Thread::key_t trace_key;
};

#endif  /* __tracelib__ */
