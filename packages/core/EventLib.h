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

#ifndef __eventlib__
#define __eventlib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Dictionary.h"
#include "List.h"
#include "Table.h"

#include <atomic>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define ORIGIN  0

typedef enum {
    DEBUG               = 0,
    INFO                = 1,
    WARNING             = 2,
    ERROR               = 3,
    CRITICAL            = 4,
    INVALID_EVENT_LEVEL = 5
} event_level_t;

#define mlog(lvl,...) EventLib::logMsg(__FILE__,__LINE__,lvl,__VA_ARGS__)

#ifdef __tracing__
#define start_trace(lvl, parent, name, fmt, ...) EventLib::startTrace(parent, name, lvl, fmt, __VA_ARGS__)
#define stop_trace(lvl, id) EventLib::stopTrace(id, lvl)
#else
#define start_trace(lvl,parent,...) {ORIGIN}; (void)lvl; (void)parent;
#define stop_trace(lvl,id,...) {(void)lvl; (void)id;}
#endif

#define up_metric(lvl, id, val) {EventLib::updateMetric(id, val); EventLib::generateMetric(id, lvl);}

/******************************************************************************
 * EVENT LIBRARY CLASS
 ******************************************************************************/

class EventLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_NAME_SIZE = 32;
        static const int MAX_ATTR_SIZE = 512;
        static const int MAX_METRICS = 128;
        static const int32_t INVALID_METRIC = -1;

        static const char* rec_type;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int64_t     systime;                    // time of event
            int64_t     tid;                        // task id
            uint32_t    id;                         // event id
            uint32_t    parent;                     // parent event id
            uint16_t    flags;                      // flags_t
            uint8_t     type;                       // type_t
            uint8_t     level;                      // event_level_t
            char        ipv4[SockLib::IPV4_STR_LEN];// ip address of local host            
            char        name[MAX_NAME_SIZE];        // name of event
            char        attr[MAX_ATTR_SIZE];        // attributes associated with event
        } event_t;

        typedef enum {
            START   = 0x01,
            STOP    = 0x02
        } flags_t;

        typedef enum {
            LOG     = 0x01,
            TRACE   = 0x02,
            METRIC  = 0x04
        } type_t;

        typedef struct {
            int32_t     id;
            const char* name;
            const char* attr;
            double      value;
        } metric_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void             init            (const char* eventq);
        static void             deinit          (void);

        static  bool            setLvl          (type_t type, event_level_t lvl);
        static  event_level_t   getLvl          (type_t type);
        static  const char*     lvl2str         (event_level_t lvl);
        static  const char*     type2str        (type_t type);

        static uint32_t         startTrace      (uint32_t parent, const char* name, event_level_t lvl, const char* fmt, ...) VARG_CHECK(printf, 4, 5);
        static void             stopTrace       (uint32_t id, event_level_t lvl);
        static void             stashId         (uint32_t id);
        static uint32_t         grabId          (void);

        static void             logMsg          (const char* file_name, unsigned int line_number, event_level_t lvl, const char* fmt, ...) VARG_CHECK(printf, 4, 5);

        static int32_t          registerMetric  (const char* metric_name, const char* fmt, ...) VARG_CHECK(printf, 2, 3);
        static void             updateMetric    (int32_t id, double value);
        static void             generateMetric  (int32_t id, event_level_t lvl);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int              sendEvent   (event_t* event, int attr_size);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static int rec_type_size;

        static std::atomic<uint32_t> trace_id;
        static Thread::key_t trace_key;
        
        static event_level_t log_level;
        static event_level_t trace_level;
        static event_level_t metric_level;

        static Mutex metric_mut;
        static std::atomic<int32_t> metric_id;
        static Dictionary<List<int32_t,MAX_METRICS>*> metric_ids_from_attr;
        static Dictionary<int32_t> metric_ids_from_name;
        static Table<metric_t, int32_t> metric_vals;
};

#endif  /* __eventlib__ */
