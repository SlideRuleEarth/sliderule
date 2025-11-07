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
#include <atomic>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define mlog(lvl,...) EventLib::logMsg(__FILE__,__LINE__,lvl,__VA_ARGS__)

#ifdef __tracing__
#define start_trace(lvl,parent,name,fmt,...) EventLib::startTrace(parent,name,lvl,fmt,__VA_ARGS__)
#define stop_trace(lvl,id) EventLib::stopTrace(id, lvl)
#else
#define start_trace(lvl,parent,...) {ORIGIN}; (void)lvl; (void)parent;
#define stop_trace(lvl,id,...) {(void)lvl; (void)id;}
#endif

#define telemeter(lvl,tlm) EventLib::sendTlm(lvl,tlm)

#define alert(lvl,code,outq,active,...) EventLib::sendAlert(lvl,code,outq,active,__VA_ARGS__)

/******************************************************************************
 * EVENT LIBRARY CLASS
 ******************************************************************************/

class EventLib
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_MSG_STR = 1024;
        static const int MAX_SRC_STR = 32;
        static const int MAX_NAME_STR = 32;
        static const int MAX_ATTR_STR = 1024;
        static const int MAX_TLM_STR = 32;
        static const int MAX_ALERT_STR = 256;
        static const int MAX_IPV4_STR = 16;

        static const char* logRecType;
        static const char* traceRecType;
        static const char* telemetryRecType;
        static const char* alertRecType;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int64_t     time;                   // time of event
            uint32_t    level;                  // event_level_t
            char        ipv4[MAX_IPV4_STR];     // ip address of local host
            char        source[MAX_SRC_STR];    // source filename and line
            char        message[MAX_MSG_STR];   // caller defined string
        } log_t;

        typedef struct {
            int64_t     time;                   // time of event
            int64_t     tid;                    // task id
            uint32_t    id;                     // event id
            uint32_t    parent;                 // parent event id
            uint32_t    flags;                  // flags_t
            uint32_t    level;                  // event_level_t
            char        ipv4[MAX_IPV4_STR];     // ip address of local host
            char        name[MAX_NAME_STR];     // name of event
            char        attr[MAX_ATTR_STR];     // attributes associated with event
        } trace_t;

        typedef struct {
            int64_t     time;                   // time of event
            int32_t     code;                   // alert codes
            uint32_t    level;                  // event_level_t
            float       duration;               // seconds
            double      latitude;               // area of interest (single point representing area)
            double      longitude;              // area of interest (single point representing area)
            char        source_ip[MAX_TLM_STR]; // ip address of local host
            char        endpoint[MAX_TLM_STR];  // server-side API
            char        client[MAX_TLM_STR];    // Python Client, Web Client, etc
            char        account[MAX_TLM_STR];   // username
            char        version[MAX_TLM_STR];   // sliderule version
        } telemetry_t;

        typedef struct {
            int32_t     code;
            uint32_t    level;
            char        text[MAX_ALERT_STR];
        } alert_t;

        typedef enum {
            START       = 0x01,
            STOP        = 0x02
        } flags_t;

        typedef enum {
            LOG         = 0x01,
            TRACE       = 0x02,
            TELEMETRY   = 0x04,
            ALERT       = 0x08
        } type_t;

        struct tlm_input_t {
            int32_t             code = RTE_STATUS;
            float               duration = 0.0;
            double              latitude = 0.0;
            double              longitude = 0.0;
            const char*         source_ip = NULL;
            const char*         endpoint = NULL;
            const char*         client = NULL;
            const char*         account = NULL;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void             init            (const char* eventq);
        static void             deinit          (void);

        static  const char*     lvl2str         (event_level_t lvl);
        static  const char*     lvl2str_lc      (event_level_t lvl);
        static  const char*     type2str        (type_t type);

        static bool             logMsg          (const char* file_name, unsigned int line_number, event_level_t lvl, const char* msg_fmt, ...) VARG_CHECK(printf, 4, 5);

        static uint32_t         startTrace      (uint32_t parent, const char* name, event_level_t lvl, const char* attr_fmt, ...) VARG_CHECK(printf, 4, 5);
        static void             stopTrace       (uint32_t id, event_level_t lvl);
        static void             stashId         (uint32_t id);
        static uint32_t         grabId          (void);

        static bool             sendTlm         (event_level_t lvl, const tlm_input_t& tlm);

        static bool             sendAlert       (event_level_t lvl, int code, void* rspsq, const std::atomic<bool>* active, const char* errmsg, ...) VARG_CHECK(printf, 5, 6);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static std::atomic<uint32_t> trace_id;
        static Thread::key_t trace_key;
};

#endif  /* __eventlib__ */
