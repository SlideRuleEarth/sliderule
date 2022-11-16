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

#include "MsgQ.h"
#include "OsApi.h"
#include "List.h"
#include "StringLib.h"
#include "TcpSocket.h"
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

        static const int MAX_RQST_BUF_LEN   = 0x10000; // 64K
        static const int MAX_RSPS_BUF_LEN   = 0x100000; // 1M
        static const int MAX_UNBOUNDED_RSPS = 1048576;
        static const int MAX_URL_LEN        = 1024;
        static const int MAX_TIMEOUTS       = 5;
        static const int MAX_DIGITS         = 10;

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            EndpointObject::code_t code;
            char* response;
            long size;
        } rsps_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate       (lua_State* L);

                        HttpClient      (lua_State* L, const char* _ip_addr, int _port);
                        HttpClient      (lua_State* L, const char* url);
                        ~HttpClient     (void);

        rsps_t          request         (EndpointObject::verb_t verb, const char* resource, const char* data, bool keep_alive, Publisher* outq, int timeout=SYS_TIMEOUT);
        const char*     getIpAddr       (void);
        int             getPort         (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            EndpointObject::verb_t      verb;
            const char*                 resource;
            const char*                 data;
            Publisher*                  outq;
        } rqst_t;

        typedef struct {
            char*                       key;
            char*                       value;
        } hdr_kv_t;

        typedef struct {
            EndpointObject::code_t      code;
            const char*                 msg;
        } status_line_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                            active;
        Thread*                         requestPid;
        Publisher*                      requestPub;
        TcpSocket*                      sock;
        char*                           ipAddr;
        int                             port;
        char                            rqstBuf[MAX_RQST_BUF_LEN];
        char                            rspsBuf[MAX_RSPS_BUF_LEN];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        TcpSocket*      initializeSocket    (const char* _ip_addr, int _port);
        bool            makeRequest         (EndpointObject::verb_t verb, const char* resource, const char* data, bool keep_alive);
        rsps_t          parseResponse       (Publisher* outq, int timeout);
        long            parseLine           (int start, int end);
        status_line_t   parseStatusLine     (int start, int term);
        hdr_kv_t        parseHeaderLine     (int start, int term);
        const char*     parseChunkHeaderLine(int start, int term);

        static void*    requestThread       (void* parm);

        static int      luaRequest          (lua_State* L);
        static int      luaConnected        (lua_State* L);
};

#endif  /* __http_client__ */
