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

#ifndef __rest_server__
#define __rest_server__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <cesanta/mongoose.h>
#include <atomic>

#include "core.h"
#include "mongoose.h"


/******************************************************************************
 * REST SERVER CLASS
 ******************************************************************************/

class RestServer: public LuaObject
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
        static const char*  getEndpoint (const char* url);
        static long         getUniqueId (char id_str[REQUEST_ID_LEN], const char* name);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    RestServer     (lua_State* L, const char* _port, size_t num_threads);
        virtual     ~RestServer    (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static std::atomic<uint32_t>    requestId;

        const char*                     port;
        size_t                          numThreads;
        bool                            active;
        Thread*                         serverPid;
    
        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     sourceHandler       (struct mg_connection *nc, struct http_message *hm);
        static void     engineHandler       (struct mg_connection *nc, struct http_message *hm);

        static void     serverHandler       (struct mg_connection *nc, int ev, void *ev_data);
        static void*    serverThread        (void* parm);
};

#endif  /* __rest_server__ */
