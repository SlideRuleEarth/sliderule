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

/*
 * TODO: Clean up semaphores from signal and waiton
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaInterpreter.h"
#include "core.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaInterpreter::TYPE = "LuaInterpreter";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* LuaInterpreter::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE], bool safe)
{
    return new LuaInterpreter(cmd_proc, name, argc, &argv[0], safe);
}

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* LuaInterpreter::createSafeObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    return createObject(cmd_proc, name, argc, argv, true);
}

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* LuaInterpreter::createUnsafeObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    return createObject(cmd_proc, name, argc, argv, false);
}

/*----------------------------------------------------------------------------
 * abortHook
 *
 *    Notes:  this hook currently only aborts the interpreter, but it could be extended
 --             with much more functionality like stepping through a script
 *----------------------------------------------------------------------------*/
void LuaInterpreter::abortHook (lua_State *L, lua_Debug *ar)
{
    (void)ar;

    /* Default to Aborting */
    bool abort = true;

    /* Check if Interpreter still Active -  in which case don't abort */
    lua_pushstring(L, LuaEngine::LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = (LuaEngine*)lua_touserdata(L, -1);
    if(li) abort = !li->isActive();

    /* If Aborting */
    if(abort)
    {
        luaL_error(L, "Interpreter no longer active - aborting!\n");
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
LuaInterpreter::LuaInterpreter(CommandProcessor* cmd_proc, const char* obj_name, int lua_argc, char lua_argv[][MAX_CMD_SIZE], bool safe):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    LuaEngine::luaStepHook hook = NULL;
    if(safe) hook = abortHook;

    /* Start Interpreter Thread */
    luaEngine = new LuaEngine(obj_name, lua_argc, lua_argv, hook);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
LuaInterpreter::~LuaInterpreter(void)
{
    delete luaEngine;
}

