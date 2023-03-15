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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/


#include "LuaScript.h"
#include "LuaEngine.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaScript::OBJECT_TYPE = "LuaScript";
const char* LuaScript::LuaMetaName = "LuaScript";
const struct luaL_Reg LuaScript::LuaMetaTable[] = {
    {"active",      luaActive},
    {"result",      luaResult},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - script(<script>, <arg>)
 *----------------------------------------------------------------------------*/
int LuaScript::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* script = getLuaString(L, 1);
        const char* arg = getLuaString(L, 2, true, NULL);

        /* Return Lua Script Object */
        return createLuaObject(L, new LuaScript(L, script, arg));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating LuaScript: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
LuaScript::LuaScript(lua_State* L, const char* script, const char* arg):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(script);

    /* Sanitize */
    if(script[0] != ' ' && script[0] != '/')
    {
        SafeString safe_filename("%s", script);
        safe_filename.replace("..", "_");
        SafeString safe_pathname("%s%c%s.lua", CONFDIR, PATH_DELIMETER, safe_filename.str());
        engine = new LuaEngine(safe_pathname.str(), arg, ORIGIN, LuaEngine::abortHook, false);
    }
    else
    {
        engine = NULL;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
LuaScript::~LuaScript(void)
{
    if(engine) delete engine;
}

/*----------------------------------------------------------------------------
 * luaActive - :active()
 *----------------------------------------------------------------------------*/
int LuaScript::luaActive (lua_State* L)
{
    try
    {
        /* Get Self */
        LuaScript* lua_obj = (LuaScript*)getLuaSelf(L, 1);

        /* Check Engine */
        if(lua_obj->engine)
        {
            /* Return Is Active */
            return returnLuaStatus(L, lua_obj->engine->isActive());
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "engine does not exist");
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error checking srcipt status: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaResult - :result()
 *----------------------------------------------------------------------------*/
int LuaScript::luaResult (lua_State* L)
{
    try
    {
        /* Get Self */
        LuaScript* lua_obj = (LuaScript*)getLuaSelf(L, 1);

        /* Check Engine */
        if(lua_obj->engine)
        {
            /* Get Engine Results */
            const char* result = lua_obj->engine->getResult();

            /* Push Results */
            lua_pushstring(L, result);
            return returnLuaStatus(L, true, 2);
        }
        else
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "engine does not exist");
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error returning script result: %s", e.what());
        return returnLuaStatus(L, false);
    }
}
