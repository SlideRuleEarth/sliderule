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

#ifndef __route_handler__
#define __route_handler__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "core.h"
#include "pistache.h"

#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>

using namespace Pistache;

/******************************************************************************
 * ROUTE HANDLER CLASS (PURE VIRTUAL)
 ******************************************************************************/

class RouteHandler: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OBJECT_TYPE;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef int (*handler_t) (const Rest::Request& request, Http::ResponseWriter response);

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual handler_t   getHandler  (void) = 0;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                    RouteHandler     (lua_State* L, const char* meta_name, const struct luaL_Reg meta_table[]);
        virtual     ~RouteHandler    (void);
};

#endif  /* __route_handler__ */
