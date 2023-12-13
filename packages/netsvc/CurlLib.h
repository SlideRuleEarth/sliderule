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

#ifndef __curl_lib__
#define __curl_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "EndpointObject.h"
#include "OsApi.h"
#include "MsgQ.h"

/******************************************************************************
 * cURL LIBRARY CLASS
 ******************************************************************************/

class CurlLib
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void         init            (void);
        static void         deinit          (void);

        static long         request         (EndpointObject::verb_t verb, const char* url, const char* data, const char** response, int* size, bool verify_peer=false, bool verify_hostname=false, List<const char*>* headers=NULL);
        static long         get             (const char* url, const char* data, const char** response, int* size, bool verify_peer=false, bool verify_hostname=false);
        static long         put             (const char* url, const char* data, const char** response, int* size, bool verify_peer=false, bool verify_hostname=false);
        static long         post            (const char* url, const char* data, const char** response, int* size, bool verify_peer=false, bool verify_hostname=false);
        static long         postAsStream    (const char* url, const char* data, Publisher* outq, bool with_terminator);
        static long         postAsRecord    (const char* url, const char* data, Publisher* outq, bool with_terminator, int timeout, bool* active=NULL);
        static int          getHeaders      (lua_State* L, int index, List<const char*>& header_list);
        static int          luaGet          (lua_State* L);
        static int          luaPut          (lua_State* L);
        static int          luaPost         (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int EXPECTED_RESPONSE_SEGMENTS = 16;
        static const int CONNECTION_TIMEOUT = 10L; // seconds
        static const int DATA_TIMEOUT = 60L; // seconds
        static const int EXPECTED_MAX_HEADERS = 8;

        static const int RECOBJ_HDR_SIZE = 8; // bytes

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            char* data;
            size_t size;
        } data_t;

        typedef struct {
            uint8_t     hdr_buf[RECOBJ_HDR_SIZE];
            uint32_t    hdr_index;
            uint32_t    rec_size;
            uint32_t    rec_index;
            uint8_t*    rec_buf;
            Publisher*  outq;
            const char* url;
            bool*       active;
        } parser_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     combineResponse (List<data_t>* rsps_set, const char** response, int* size);
        static size_t   postRecords     (void *buffer, size_t size, size_t nmemb, void *userp);
        static size_t   postData        (void *buffer, size_t size, size_t nmemb, void *userp);
        static size_t   writeData       (void *buffer, size_t size, size_t nmemb, void *userp);
        static size_t   readData        (void* buffer, size_t size, size_t nmemb, void *userp);
};

#endif  /* __curl_lib__ */
