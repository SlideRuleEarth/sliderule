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
    {"log",         LuaLibraryCmd::lcmd_log},
    {"type",        LuaLibraryCmd::lcmd_type},
    {"waiton",      LuaLibraryCmd::lcmd_waiton},
    {"signal",      LuaLibraryCmd::lcmd_signal},
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
                mlog(CRITICAL, "Command verification timed out\n");
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
 * lcmd_log
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_log (lua_State* L)
{
    bool status = false;

    if(lua_isstring(L, 1)) // check log level parameter
    {
        if(lua_isstring(L, 2)) // check message to log
        {
            log_lvl_t lvl;
            if(LogLib::str2lvl(lua_tostring(L, 1), &lvl))
            {
                mlog(lvl, "%s", lua_tostring(L, 2));
                status = true;
            }
        }
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

    if(lua_isstring(L, 1)) // check log level parameter
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
 * lcmd_waiton
 *
 *   Note that this returns true/false even though there is a timeout.  The reason is
 *   that there is no error other than a timeout, so timeout and false are synonymous
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_waiton (lua_State* L)
{
    bool status = false;

    /* Get String to Post */
    const char* signal_name = lua_tostring(L, 1);  /* get argument 1 */
    const int timeout_ms = (int)lua_tonumber(L, 2); /* get argument 2 */

    /* Get Lua Engine Object */
    lua_pushstring(L, LuaEngine::LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = (LuaEngine*)lua_touserdata(L, -1);
    if(li)
    {
        status = li->waitOn(signal_name, timeout_ms);
    }
    else
    {
        mlog(ERROR, "Unable to locate lua engine object\n");
    }

    /* Return Status to Lua */
    lua_pushboolean(L, status); /* push result status */
    return 1;                   /* number of results */
}

/*----------------------------------------------------------------------------
 * lcmd_signal
 *----------------------------------------------------------------------------*/
int LuaLibraryCmd::lcmd_signal (lua_State* L)
{
    bool status = false;

    /* Get String to Post */
    const char* signal_name = lua_tostring(L, 1);  /* get argument 1 */

    /* Get Lua Engine Object */
    lua_pushstring(L, LuaEngine::LUA_SELFKEY);
    lua_gettable(L, LUA_REGISTRYINDEX); /* retrieve value */
    LuaEngine* li = (LuaEngine*)lua_touserdata(L, -1);
    if(li)
    {
        status = li->signal(signal_name);
    }
    else
    {
        mlog(ERROR, "Unable to locate lua engine object\n");
    }

    /* Return Status to Lua */
    lua_pushboolean(L, status); /* push result status */
    return 1;                   /* number of results */
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
    if(status)  mlog(CRITICAL, "Successfully waited for object %s to %s\n", obj_name, exists ? "be created" : "be closed");
    else        mlog(CRITICAL, "Failed to wait for object %s to %s\n", obj_name, exists ? "be created" : "be closed");
    lua_pushboolean(L, status); /* push result status */
    return 1;                   /* number of results */
}
