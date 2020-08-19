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

#ifndef __lua_library_sys__
#define __lua_library_sys__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "Ordering.h"

#include <lua.h>

/******************************************************************************
 * LUA LIBRARY SYS CLASS
 ******************************************************************************/

class LuaLibrarySys
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_SYSLIBNAME;
        static const struct luaL_Reg sysLibs [];

        static const int LUA_COMMAND_TIMEOUT = 30000; // milliseconds

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     lsys_init           (void);
        static int      luaopen_syslib      (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      lsys_help           (lua_State* L);
        static int      lsys_version        (lua_State* L);
        static int      lsys_quit           (lua_State* L);
        static int      lsys_abort          (lua_State* L);
        static int      lsys_wait           (lua_State* L);
        static int      lsys_log            (lua_State* L);
        static int      lsys_lsmsgq         (lua_State* L);
        static int      lsys_type           (lua_State* L);
        static int      lsys_setstddepth    (lua_State* L);
};

#endif  /* __lua_library_sys__ */
