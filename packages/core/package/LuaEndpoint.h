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

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        static const char* ENDPOINT_MAIN;
        static const char* ENDPOINT_LOGGING;
        static const char* ENDPOINT_ROLES;
        static const char* ENDPOINT_SIGNED;
        static const char* ENDPOINT_OUTPUTS;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            event_level_t       log_level;          // ENDPOINT_LOGGING
            vector<string>      allowed_roles;      // ENDPOINT_ROLES
            bool                signature_required; // ENDPOINT_SIGNED
            vector<content_t>   supported_outputs;  // ENDPOINT_OUTPUTS
        } endpoint_t;

         /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);
        static void         defaultHandler      (Request* request, LuaEngine* engine, content_t selected_output, const char* arguments);

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            LuaEndpoint         (lua_State* L);
                            ~LuaEndpoint        (void) override;

        void                handleRequest       (Request* request) override;

        static endpoint_t   loadLuaScript       (Request* request, LuaEngine* engine, const string& script);
        static void         logRequest          (Request* request, const endpoint_t& endpoint);
        static void         checkRole           (Request* request, const endpoint_t& endpoint);
        static void         checkSignature      (Request* request, const endpoint_t& endpoint);
        static content_t    selectOutput        (Request* request, const endpoint_t& endpoint, const string& extension);
        static void         checkMemoryUsage    (Request* request);
        static void         executeEndpoint     (Request* request, LuaEngine* engine, content_t selected_output, const string& arguments);

        static void*        requestThread       (void* parm);
};

#endif  /* __lua_endpoint__ */
