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

#ifndef __pistache_server__
#define __pistache_server__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/description.h>
#include <pistache/endpoint.h>

#include "core.h"

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

        static StringLib::String serverHead;

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
