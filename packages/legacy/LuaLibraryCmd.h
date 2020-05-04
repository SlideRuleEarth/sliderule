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

#ifndef __lua_library_cmd__
#define __lua_library_cmd__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandProcessor.h"

#include <lua.h>

/******************************************************************************
 * LUA LIBRARY CMD
 ******************************************************************************/

class LuaLibraryCmd
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_CMDLIBNAME;
        static const int LUA_COMMAND_TIMEOUT = 30000; // milliseconds

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     lcmd_init           (CommandProcessor* cmd_proc);
        static int      luaopen_cmdlib      (lua_State* L);

    private:
        
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const struct luaL_Reg cmdLibs [];
        static CommandProcessor* cmdProc;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      lcmd_exec           (lua_State* L);
        static int      lcmd_script         (lua_State* L);
        static int      lcmd_log            (lua_State* L);
        static int      lcmd_type           (lua_State* L);
        static int      lcmd_waiton         (lua_State* L);
        static int      lcmd_signal         (lua_State* L);        
        static int      lcmd_stopuntil      (lua_State* L);        
};

#endif  /* __lua_library_cmd__ */
