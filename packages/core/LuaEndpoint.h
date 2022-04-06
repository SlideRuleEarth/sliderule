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

#ifndef __lua_endpoint__
#define __lua_endpoint__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "EndpointObject.h"
#include "OsApi.h"
#include "StringLib.h"
#include "Dictionary.h"
#include "MsgQ.h"
#include "LuaObject.h"
#include "RecordObject.h"

/******************************************************************************
 * PISTACHE SERVER CLASS
 ******************************************************************************/

class LuaEndpoint: public EndpointObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const char* EndpointExceptionRecType;
        static const RecordObject::fieldDef_t EndpointExceptionRecDef[];
        
        static const double DEFAULT_NORMAL_REQUEST_MEMORY_THRESHOLD;
        static const double DEFAULT_STREAM_REQUEST_MEMORY_THRESHOLD;

        static const int MAX_SOURCED_RESPONSE_SIZE = 1048576; // 1M
        static const int MAX_RESPONSE_TIME_MS = 5000;
        static const int INITIAL_NUM_ENDPOINTS = 32;
        static const int MAX_EXCEPTION_TEXT_SIZE = 256;
        static const char* RESPONSE_QUEUE;
        static const char* ALL_ENDPOINTS;
        static const char* HITS_METRIC;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Response Exception Record */
        typedef struct {
            int32_t code;
            char    text[MAX_EXCEPTION_TEXT_SIZE];
        } response_exception_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static SafeString serverHead;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

       static bool         init             (void);
       static int          luaCreate        (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaEndpoint     (lua_State* L, double normal_mem_thresh, double stream_mem_thresh);
        virtual             ~LuaEndpoint    (void);

        static void*        requestThread   (void* parm);

        rsptype_t           handleRequest   (request_t* request) override;

        void                normalResponse  (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id);
        void                streamResponse  (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id);

        int32_t             getMetricId     (const char* endpoint);

        static int          luaMetric       (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static int32_t      totalMetricId;
        Dictionary<int32_t> metricIds;
        double              normalRequestMemoryThreshold;
        double              streamRequestMemoryThreshold;
};

#endif  /* __lua_endpoint__ */
