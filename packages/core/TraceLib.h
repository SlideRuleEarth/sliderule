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
#include "Ordering.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#ifdef LTTNG_TRACING
#define start_trace() {}
#define stop_trace() {}
#else
#define start_trace(...) {}
#define stop_trace(...) {}
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

        static const int MAX_LOG_ENTRY_SIZE = 512;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            uint32_t    id;
            uint32_t    parent;
        } trace_t;

        typedef int (*logFunc_t) (const char* str, int size, void* parm);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static  okey_t      createLog   (log_lvl_t lvl, logFunc_t handler, void* parm);
        static  bool        deleteLog   (okey_t id);
        static  bool        setLevel    (okey_t id, log_lvl_t lvl);
        static  log_lvl_t   getLevel    (okey_t id);
        static  int         getLvlCnts  (log_lvl_t lvl);
        static  bool        str2lvl     (const char* str, log_lvl_t* lvl);
        static  void        logMsg      (const char* file_name, unsigned int line_number, log_lvl_t lvl, const char* format_string, ...) VARG_CHECK(printf, 4, 5);
        static  void        initLib     (void);
        static  void        deinitLib   (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct
        {
            okey_t      id;
            log_lvl_t   level;
            logFunc_t   handler;
            void*       parm;
        } log_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static okey_t           logIdPool;
        static Ordering<log_t>  logList;
        static Mutex            logMut;
        static int              logLvlCnts[RAW + 1];
};

#endif  /* __loglib__ */
