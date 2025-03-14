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

#ifndef __endpoint_object__
#define __endpoint_object__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "StringLib.h"
#include "Dictionary.h"
#include "LuaObject.h"

/******************************************************************************
 * ENDPOINT OBJECT CLASS
 ******************************************************************************/

class EndpointObject: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_HDR_SIZE = MAX_STR_SIZE;
        static const int EXPECTED_MAX_HEADER_FIELDS = 32;
        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Enumerations
         *--------------------------------------------------------------------*/

        typedef enum {
            GET,
            HEAD,
            POST,
            PUT,
            DELETE,
            TRACE,
            OPTIONS,
            CONNECT,
            RAW, // used to purposefully send raw strings
            UNRECOGNIZED
        } verb_t;

        typedef enum {
            OK = 200,
            Created = 201,
            No_Content = 204,
            Bad_Request = 400,
            Unauthorized = 401,
            Not_Found = 404,
            Method_Not_Allowed = 405,
            Request_Timeout = 408,
            Internal_Server_Error = 500,
            Method_Not_Implemented = 501,
            Service_Unavailable = 503
        } code_t;

        typedef enum {
            INVALID = -1,
            NORMAL = 0,
            STREAMING = 1
        } rsptype_t;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef Dictionary<string*> HeaderDictionary;

        /*--------------------------------------------------------------------
         * Request Subclass
         *--------------------------------------------------------------------*/

        class Request
        {
            public:

                const char*         path;
                const char*         resource;
                verb_t              verb;
                const char*         version;
                HeaderDictionary    headers;
                uint8_t*            body;
                long                length; // of body
                uint32_t            trace_id;
                const char*         id; // must be unique

                explicit Request (const char* _id);
                ~Request (void);
        };

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct {
            EndpointObject*     endpoint;
            Request*            request;
            bool                streaming;
        } info_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            EndpointObject      (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
                            ~EndpointObject     (void) override;

        static verb_t       str2verb            (const char* str);
        static const char*  verb2str            (verb_t verb);
        static code_t       str2code            (const char* str);
        static const char*  code2str            (code_t code);
        static int          buildheader         (char hdr_str[MAX_HDR_SIZE], code_t code, const char* content_type=NULL, int content_length=0, const char* transfer_encoding=NULL, const char* server=NULL);

        virtual rsptype_t   handleRequest       (Request* request) = 0;

};

#endif  /* __endpoint_object__ */
