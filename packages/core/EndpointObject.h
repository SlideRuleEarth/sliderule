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
            UNRECOGNIZED
        } verb_t;

        typedef enum {
            OK = 200,
            Bad_Request = 400,
            Not_Found = 404,
            Method_Not_Allowed = 405,
            Request_Timeout = 408,
            Method_Not_Implemented = 501
        } code_t;

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

        virtual code_t      handleRequest       (const char* id, const char* url, verb_t verb, Dictionary<const char*>& headers, const char* body, EndpointObject* self) = 0;
};

#endif  /* __endpoint_object__ */
