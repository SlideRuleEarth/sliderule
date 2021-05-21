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

/*
 * TODO: Clean up semaphores from signal and waiton
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaInterpreter.h"
#include "core.h"

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
 *            with much more functionality like stepping through a script
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
    luaEngine = new LuaEngine(obj_name, lua_argc, lua_argv, ORIGIN, hook);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
LuaInterpreter::~LuaInterpreter(void)
{
    delete luaEngine;
}

