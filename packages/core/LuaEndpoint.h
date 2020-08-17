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

        static const int MAX_RESPONSE_TIME_MS = 5000;
        static const char* RESPONSE_QUEUE;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static SafeString ServerHeader;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate       (lua_State* L);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaEndpoint     (lua_State* L);
        virtual             ~LuaEndpoint    (void);

        static void*        requestThread   (void* parm);

        rsptype_t           handleRequest   (request_t* request) override;

        void                returnResponse  (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id);
        void                streamResponse  (const char* scriptpath, const char* body, Publisher* rspq, uint32_t trace_id);

        static const char*  sanitize        (const char* filename);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Mutex               pidMut;
        Dictionary<Thread*> pidTable;
};

#endif  /* __lua_endpoint__ */
