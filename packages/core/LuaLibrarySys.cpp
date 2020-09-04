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

#include "LuaLibrarySys.h"
#include "LuaEngine.h"
#include "core.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaLibrarySys::LUA_SYSLIBNAME = "sys";
const struct luaL_Reg LuaLibrarySys::sysLibs [] = {
    {"version",     LuaLibrarySys::lsys_version},
    {"quit",        LuaLibrarySys::lsys_quit},
    {"abort",       LuaLibrarySys::lsys_abort},
    {"wait",        LuaLibrarySys::lsys_wait},
    {"log",         LuaLibrarySys::lsys_log},
    {"lsmsgq",      LuaLibrarySys::lsys_lsmsgq},
    {"type",        LuaLibrarySys::lsys_type},
    {"setstddepth", LuaLibrarySys::lsys_setstddepth},
    {"setiosz",     LuaLibrarySys::lsys_setiosize},
    {"getiosz",     LuaLibrarySys::lsys_getiosize},
    {"lsdev",       DeviceObject::luaList},
    {NULL,          NULL}
};

/******************************************************************************
 * SYSTEM LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * lsys_init
 *----------------------------------------------------------------------------*/
void LuaLibrarySys::lsys_init (void)
{
}

/*----------------------------------------------------------------------------
 * luaopen_cmdlib
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::luaopen_syslib (lua_State *L)
{
    luaL_newlib(L, sysLibs);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_version
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_version (lua_State* L)
{
    mlog(RAW, "SlideRule Version: %s\n\n", BINID);
    mlog(RAW, "Build Information: %s\n\n", BUILDINFO);

    /* Return Version String to Lua */
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * lsys_quit
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_quit (lua_State* L)
{
    setinactive();

    /* Return Status to Lua */
    lua_pushboolean(L, true);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_abort
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_abort (lua_State* L)
{
    (void)L;

    quick_exit(0);

    return 0;
}

/*----------------------------------------------------------------------------
 * lsys_wait
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_wait (lua_State* L)
{
    /* Get Seconds to Wait */
    int secs = 0;
    if(lua_isnumber(L, 1))
    {
        secs = lua_tonumber(L, 1);
    }
    else
    {
        mlog(CRITICAL, "Incorrect parameter type for seconds to wait\n");
        lua_pushboolean(L, false); /* push result as fail */
        return 1;
    }

    /* Wait */
    LocalLib::sleep(secs);

    /* Return Success */
    lua_pushboolean(L, true);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_log - .log(<level>, <message>)
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_log (lua_State* L)
{
    bool status = false;

    log_lvl_t lvl;
    if(lua_isinteger(L, 1))
    {
        lvl = (log_lvl_t)lua_tointeger(L, 1);
        status = true;
    }
    else if(lua_isstring(L, 1)) // check log level parameter
    {
        if(LogLib::str2lvl(lua_tostring(L, 1), &lvl))
        {
            status = true;
        }
    }

    if(status && lua_isstring(L, 2))
    {
        mlog(lvl, "%s", lua_tostring(L, 2));
    }
    else
    {
        status = false;
    }

    /* Return Status to Lua */
    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_lsmsgq
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_lsmsgq (lua_State* L)
{
    (void)L;

    int num_msgqs = MsgQ::numQ();
    if(num_msgqs > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[num_msgqs];
        int numq = MsgQ::listQ(msgQs, num_msgqs);
        mlog(RAW, "\n");
        for(int i = 0; i < numq; i++)
        {
            mlog(RAW,"MSGQ: %40s %8d %9s %d\n",
                msgQs[i].name, msgQs[i].len, msgQs[i].state,
                msgQs[i].subscriptions);
        }
        mlog(RAW, "\n");
        delete [] msgQs;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * lsys_type - TODO - needs to handle userdata types
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_type (lua_State* L)
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

        if(obj_type == NULL)
        {
            char errstr[MAX_STR_SIZE];
            StringLib::format(errstr, MAX_STR_SIZE, "Object %s not registered, unable to provide type!\n", obj_name);
            return luaL_error(L, errstr);
        }
    }
    else if(lua_isuserdata(L, 1))
    {
        obj_type = "LuaObject";
    }
    else
    {
        obj_type = "Unknown";
    }

    /* Return Status to Lua */
    lua_pushstring(L, obj_type); /* push object type */
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_setstddepth
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_setstddepth (lua_State* L)
{
    int depth = 0;
    if(lua_isnumber(L, 1))
    {
        depth = lua_tonumber(L, 1);
    }
    else
    {
        mlog(CRITICAL, "Standard queue depth must be a number\n");
        lua_pushboolean(L, false); /* push result as fail */
        return 1;
    }

    /* Set Standard Depth */
    MsgQ::setStdQDepth(depth);

    /* Return Success */
    lua_pushboolean(L, true);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_setiosize
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_setiosize (lua_State* L)
{
    bool status = false;

    if(!lua_isnumber(L, 1))
    {
        mlog(CRITICAL, "I/O maximum size must be a number\n");
    }
    else
    {
        /* Set I/O Size */
        int size = lua_tonumber(L, 1);
        status = LocalLib::setIOMaxsize(size);
    }

    /* Return Status */
    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_getiosize
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_getiosize (lua_State* L)
{
    lua_pushnumber(L, LocalLib::getIOMaxsize());
    return 1;
}


