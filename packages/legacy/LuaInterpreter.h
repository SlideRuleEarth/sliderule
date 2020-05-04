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

#ifndef __lua_interpreter__
#define __lua_interpreter__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "LuaEngine.h"
#include "OsApi.h"

/******************************************************************************
 * LUA INTERPRETER CLASS
 ******************************************************************************/

class LuaInterpreter : public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject          (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE], bool safe);
        static CommandableObject* createSafeObject      (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);
        static CommandableObject* createUnsafeObject    (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        LuaEngine* luaEngine;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        LuaInterpreter  (CommandProcessor* cmd_proc, const char* obj_name, int lua_argc=0, char lua_argv[][MAX_CMD_SIZE]=NULL, bool safe=false);
        ~LuaInterpreter (void);

        static void abortHook (lua_State *L, lua_Debug *ar);
};

#endif  /* __LUA_INTERPRETER_HPP__ */
