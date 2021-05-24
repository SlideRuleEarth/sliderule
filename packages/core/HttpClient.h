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

#ifndef __http_client__
#define __http_client__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "MsgQ.h"
#include "OsApi.h"
#include "List.h"
#include "StringLib.h"
#include "LuaEngine.h"
#include "LuaObject.h"
#include "EndpointObject.h"

/******************************************************************************
 * HTTP SERVER CLASS
 ******************************************************************************/

class HttpClient: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_RQST_DATA_LEN  = 65536;
        static const int RSPS_READ_BUF_LEN  = 65536;
        static const int MAX_TIMEOUTS       = 5;

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate       (lua_State* L);

                            HttpClient      (lua_State* L, const char* _ip_addr, int _port);
                            ~HttpClient     (void);

        const char*         getIpAddr       (void);
        int                 getPort         (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            bool                        active;
            bool                        keep_alive;
            Thread*                     pid;
            EndpointObject::verb_t      verb;
            const char*                 resource;
            const char*                 data;
            Publisher*                  outq;
            HttpClient*                 client;
        } connection_t;

        typedef struct {
            const char*                 data;
            int                         size;
        } in_place_rsps_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        List<connection_t*>             connections;
        Mutex                           connMutex;

        char*                           ipAddr;
        int                             port;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*        requestThread       (void* parm);
        static int          luaRequest          (lua_State* L);
};

#endif  /* __http_client__ */
