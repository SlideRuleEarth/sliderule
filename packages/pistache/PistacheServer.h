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

#ifndef __pistache_server__
#define __pistache_server__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "core.h"
#include "pistache.h"

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/description.h>
#include <pistache/endpoint.h>

using namespace Pistache;

/******************************************************************************
 * PISTACHE SERVER CLASS
 ******************************************************************************/

class PistacheServer: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const int REQUEST_ID_LEN = MAX_STR_SIZE;
        static const int MAX_RESPONSE_TIME_MS = 5000;
        static const char* RESPONSE_QUEUE;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static StringLib::String ServerHeader;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            GET,
            OPTIONS,
            POST,
            PUT,
            INVALID
        } verb_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate   (lua_State* L);
        static verb_t       str2verb    (const char* str);
        static const char*  sanitize    (const char* filename);

        long                getUniqueId (char id_str[REQUEST_ID_LEN]);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    PistacheServer     (lua_State* L, Address addr, size_t num_threads);
        virtual     ~PistacheServer    (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        long                            requestId;
        Mutex                           idMut;
        size_t                          numThreads;

        bool                            active;
        Thread*                         serverPid;
        std::shared_ptr<Http::Endpoint> httpEndpoint;
        Rest::Router                    router;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        void            echoHandler         (const Rest::Request& request, Http::ResponseWriter response);
        void            infoHandler         (const Rest::Request& request, Http::ResponseWriter response);
        void            sourceHandler       (const Rest::Request& request, Http::ResponseWriter response);
        void            engineHandler       (const Rest::Request& request, Http::ResponseWriter response);

        static void*    serverThread        (void* parm);
        static int      luaRoute            (lua_State* L);
};

#endif  /* __pistache_server__ */
