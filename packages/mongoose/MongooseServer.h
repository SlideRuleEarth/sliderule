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

#ifndef __mongoose_server__
#define __mongoose_server__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <cesanta/mongoose.h>
#include <atomic>

#include "core.h"
#include "mongoose.h"


/******************************************************************************
 * MONGOOSE SERVER CLASS
 ******************************************************************************/

class MongooseServer: public LuaObject
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

                    MongooseServer     (lua_State* L, const char* _port, size_t num_threads);
        virtual     ~MongooseServer    (void);

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

#endif  /* __mongoose_server__ */
