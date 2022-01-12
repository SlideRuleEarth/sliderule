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

#ifndef __http_server__
#define __http_server__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "MsgQ.h"
#include "Table.h"
#include "OsApi.h"
#include "StringLib.h"
#include "LuaObject.h"
#include "EndpointObject.h"

/******************************************************************************
 * HTTP SERVER CLASS
 ******************************************************************************/

class HttpServer: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int REQUEST_MSG_BUF_LEN        = MAX_STR_SIZE;
        static const int REQUEST_ID_LEN             = 128;
        static const int CONNECTION_TIMEOUT         = 5; // seconds
        static const int INITIAL_POLL_SIZE          = 16;
        static const int DEFAULT_MAX_CONNECTIONS    = 256;
        static const int STREAM_OVERHEAD_SIZE       = 128; // chunk size, record size, and line breaks
        static const int EXPECTED_MAX_HEADER_FIELDS = 64;

        static const char* DURATION_METRIC;

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate       (lua_State* L);

                            HttpServer      (lua_State* L, const char* _ip_addr, int _port, int max_connections);
                            ~HttpServer     (void);

        const char*         getUniqueId     (void);
        const char*         getIpAddr       (void);
        int                 getPort         (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            int                         header_index;
            bool                        header_complete;
            bool                        header_sent;
            bool                        response_complete;
            Subscriber::msgRef_t        ref;
            int                         ref_status;
            int                         ref_index;
            Subscriber*                 rspq;
            uint8_t*                    stream_buf;
            int                         stream_buf_index;
            int                         stream_buf_size;
            int                         stream_mem_size;
        } state_t;

        typedef struct {
            SafeString                  message; // raw request
            EndpointObject::request_t   request;
            state_t                     state;
            double                      start_time;
        } connection_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static std::atomic<uint64_t>    requestId;

        bool                            active;
        Thread*                         listenerPid;
        Table<connection_t*, int>       connections;

        Dictionary<EndpointObject*>     routeTable;

        char*                           ipAddr;
        int                             port;

        int32_t                         metricId;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*        listenerThread      (void* parm);

        static void         extract             (const char* url, char** endpoint, char** new_url);

        static int          luaAttach           (lua_State* L);
        static int          luaMetric           (lua_State* L);

        static int          pollHandler         (int fd, short* events, void* parm);
        static int          activeHandler       (int fd, int flags, void* parm);
        int                 onRead              (int fd);
        int                 onWrite             (int fd);
        int                 onAlive             (int fd);
        int                 onConnect           (int fd);
        int                 onDisconnect        (int fd);
};

#endif  /* __http_server__ */
