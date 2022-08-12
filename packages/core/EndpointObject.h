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
        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Types
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
            Bad_Request = 400,
            Not_Found = 404,
            Method_Not_Allowed = 405,
            Request_Timeout = 408,
            Internal_Server_Error = 500,
            Method_Not_Implemented = 501,
            Service_Unavailable = 503
        } code_t;

        typedef enum {
            NORMAL = 0,
            STREAMING = 1
        } rsptype_t;

        typedef struct {
            const char*                 id; // must be unique
            char*                       url;
            verb_t                      verb;
            Dictionary<const char*>*    headers;
            const char*                 body;
            long                        body_length;
            EndpointObject*             endpoint;
            bool                        active;
            rsptype_t                   response_type;
            Thread*                     pid;
        } request_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            EndpointObject      (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
        virtual             ~EndpointObject     (void);

        static verb_t       str2verb            (const char* str);
        static const char*  verb2str            (verb_t verb);
        static code_t       str2code            (const char* str);
        static const char*  code2str            (code_t code);
        static int          buildheader         (char hdr_str[MAX_HDR_SIZE], code_t code, const char* content_type=NULL, int content_length=0, const char* transfer_encoding=NULL, const char* server=NULL);

        virtual rsptype_t   handleRequest       (request_t* request) = 0;
};

#endif  /* __endpoint_object__ */
