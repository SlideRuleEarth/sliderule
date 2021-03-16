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

#include "LuaLibraryCmd.h"
#include "core.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaLibraryCmd::LUA_CMDLIBNAME = "cmd";

const struct luaL_Reg LuaLibraryCmd::cmdLibs [] = {
    {"exec",        LuaLibraryCmd::lcmd_exec},
    {"script",      LuaLibraryCmd::lcmd_script},
    {"type",        LuaLibraryCmd::lcmd_type},
    {"stopuntil",   LuaLibraryCmd::lcmd_stopuntil},
    {NULL, NULL}
};

CommandProcessor* LuaLibraryCmd::cmdProc = NULL;

/******************************************************************************
 * COMMAND LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lcmd_init
 *----------------------------------------------------------------------------*/
void LuaLibraryCmd::lcmd_init (CommandProcessor* cmd_proc)
{
    cmdProc = cmd_proc;
}

/*----------------------------------------------------------------------------
 * luaopen_cmdlib
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::luaopen_cmdlib (lua_State *L)
{
    luaL_newlib(L, cmdLibs);
    return 1;
}

/*----------------------------------------------------------------------------
 * lcmd_exec
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_exec (lua_State* L)
{
    int cmd_status = CommandableObject::STANDARD_CMD_ERROR;

    /* Get String to Post */
    char* str = StringLib::duplicate(lua_tostring(L, 1));  /* get argument */

    /* Get Timeout */
    int cmd_timeout = LUA_COMMAND_TIMEOUT;
    if(lua_isnumber(L, 2)) cmd_timeout = lua_tonumber(L, 2);

    /* Get Lua Engine Object */
    lua_pushstring(L, LuaEngine::LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = (LuaEngine*)lua_touserdata(L, -1);
    if(li)
    {
        /* Get Key */
        char store_key[LuaEngine::MAX_LUA_ARG];
        StringLib::format(store_key, LuaEngine::MAX_LUA_ARG, "%s_cmd_status", li->getName());

        /* Clean Command */
        char* comment_ptr = StringLib::find(str, CommandProcessor::COMMENT[0]);
        if(comment_ptr) *comment_ptr = '\0'; // discards comments

        /* Execute String */
        if(cmdProc->postCommand("%s @%s", str, store_key))
        {
            int ret_size = cmdProc->getCurrentValue(cmdProc->getName(), store_key, &cmd_status, sizeof(cmd_status), cmd_timeout, true);
            if(ret_size <= 0)
            {
                mlog(CRITICAL, "Command verification timed out");
                cmd_status = CommandableObject::CMD_VERIFY_ERROR;
            }
        }
    }

    /* Delete String */
    delete [] str;

    /* Return Status to Lua */
    lua_pushnumber(L, cmd_status);  /* push result status */
    return 1;                       /* number of results */
}

/*----------------------------------------------------------------------------
 * lcmd_script
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_script (lua_State* L)
{
    bool status = false;

    /* Get String to Post */
    const char* str = lua_tostring(L, 1);  /* get argument */

    /* Execute Script */
    if(cmdProc->executeScript(str))
    {
        status = true; // set success
    }

    /* Return Status to Lua */
    lua_pushboolean(L, status); /* push result status */
    return 1;                   /* number of results */
}

/*----------------------------------------------------------------------------
 * lcmd_type
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_type (lua_State* L)
{
    const char* obj_type = NULL;

    if(lua_isstring(L, 1)) // check event level parameter
    {
        const char* obj_name = lua_tostring(L, 1);
        if(MsgQ::existQ(obj_name))
        {
            obj_type = "MsgQ";
        }
        else if(RecordObject::isRecord(obj_name))
        {
            obj_type = "Record";
        }
        else
        {
            obj_type = cmdProc->getObjectType(obj_name);
        }

        if(obj_type == NULL)
        {
            char errstr[MAX_STR_SIZE];
            StringLib::format(errstr, MAX_STR_SIZE, "Object %s not registered, unable to provide type!\n", obj_name);
            return luaL_error(L, errstr);
        }
    }

    /* Return Status to Lua */
    lua_pushstring(L, obj_type); /* push object type */
    return 1;                    /* number of results */
}

/*----------------------------------------------------------------------------
 * lcmd_stopuntil - stopuntil(<obj name>, true | false, seconds to wait)
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_stopuntil (lua_State* L)
{
    bool status = false;

    const char* obj_name    = lua_tostring(L, 1);  /* get argument 1 */
    bool        exists      = lua_toboolean(L, 2) == 1;  /* get argument 2 - note lua_toboolean returns integer 1 or 0 instead of bool */
    int         wait_secs   = (int)lua_tonumber(L, 3);  /* get argument 3 * - note lua_tonumber returns double */

    bool        pend        = wait_secs == 0;
    bool        check       = wait_secs < 0;

    while(true)
    {
        const char* type = cmdProc->getObjectType(obj_name);
        if((type != NULL) == exists)
        {
            status = true;
            break; // condition met
        }
        else if(check)
        {
            break; // only check condition once
        }
        else if(!pend && (wait_secs-- <= 0))
        {
            break; // count only if not pending
        }
        LocalLib::sleep(1);
    }

    /* Return Status to Lua */
    if(status)  mlog(CRITICAL, "Successfully waited for object %s to %s", obj_name, exists ? "be created" : "be closed");
    else        mlog(CRITICAL, "Failed to wait for object %s to %s", obj_name, exists ? "be created" : "be closed");
    lua_pushboolean(L, status); /* push result status */
    return 1;                   /* number of results */
}
