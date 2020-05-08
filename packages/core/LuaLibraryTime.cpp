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

#include "LuaLibraryTime.h"
#include "LuaEngine.h"
#include "core.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaLibraryTime::LUA_TIMELIBNAME = "time";

const struct luaL_Reg LuaLibraryTime::timeLibs [] = {
    {"latch",    LuaLibraryTime::ltime_latch},
    {"gps",      LuaLibraryTime::ltime_getgps},
    {"gmt",      LuaLibraryTime::ltime_getgmt},
    {"gps2gmt",  LuaLibraryTime::ltime_gps2gmt},
    {"cds2gmt",  LuaLibraryTime::ltime_cds2gmt},
    {"gmt2gps",  LuaLibraryTime::ltime_gmt2gps},
    {NULL, NULL}
};

/******************************************************************************
 * COMMAND LIBRARY EXTENSION METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * ltime_init
 *----------------------------------------------------------------------------*/
void LuaLibraryTime::ltime_init (void)
{
    // do nothing
}

/*----------------------------------------------------------------------------
 * luaopen_timelib
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::luaopen_timelib (lua_State *L)
{
    luaL_newlib(L, timeLibs);
    return 1;
}

/*----------------------------------------------------------------------------
 * ltime_latch - now = time.latch()
 *
 *  now: returns monotonically incrementing number of seconds as double precision
 *       floating point number (CLOCK_MONOTONIC)
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_latch (lua_State* L)
{
    double now = TimeLib::latchtime();
    lua_pushnumber(L, now);     /* push "now" as result */
    return 1;                   /* one result */
}

/*----------------------------------------------------------------------------
 * ltime_getgps - now = time.gps()
 *
 *  now: returns number of milliseconds since GPS epoch as provided by the linux
 *        system time (CLOCK_REALTIME)
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_getgps (lua_State* L)
{
    int64_t now = TimeLib::gettimems();
    lua_pushnumber(L, (lua_Number)now); /* push "now" as result */
    return 1;                           /* one result */
}

/*----------------------------------------------------------------------------
 * ltime_getgmt - year, day, hour, minute, second, millisecond = time.gmt()
 *
 *  returns list specifying GMT time as provided by the linux system time
 *  (CLOCK_REALTIME)
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_getgmt (lua_State* L)
{
    TimeLib::gmt_time_t now = TimeLib::gettime();
    lua_pushnumber(L, now.year);                    /* push "year" as result */
    lua_pushnumber(L, now.day);                     /* push "day" as result */
    lua_pushnumber(L, now.hour);                    /* push "hour" as result */
    lua_pushnumber(L, now.minute);                  /* push "minute" as result */
    lua_pushnumber(L, now.second);                  /* push "second" as result */
    lua_pushnumber(L, now.millisecond);             /* push "millisecond" as result */
    return 6;                                       /* six results */
}

/*----------------------------------------------------------------------------
 * ltime_gps2gmt - year, day, hour, minute, second, millisecond = time.gps2gmt(gps)
 *
 *  gps: number of milliseconds since GPS epoch
 *
 *  returns list specifying GMT time as specified by the gps time supplied
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_gps2gmt (lua_State* L)
{
    const int64_t gpsms = (int64_t)lua_tonumber(L, 1);     /* get argument 1 */
    TimeLib::gmt_time_t now = TimeLib::gps2gmttime(gpsms);
    lua_pushnumber(L, now.year);                    /* push "year" as result */
    lua_pushnumber(L, now.day);                     /* push "day" as result */
    lua_pushnumber(L, now.hour);                    /* push "hour" as result */
    lua_pushnumber(L, now.minute);                  /* push "minute" as result */
    lua_pushnumber(L, now.second);                  /* push "second" as result */
    lua_pushnumber(L, now.millisecond);             /* push "millisecond" as result */
    return 6;                                       /* six results */
}

/*----------------------------------------------------------------------------
 * ltime_cds2gmt - year, day, hour, minute, second, millisecond = time.cds2gmt(day, millisecond)
 *
 *  day: number of days since GPS epoch
 *  millisecond: number of milliseconds in current day
 *
 *  returns list specifying GMT time as specified by the cds time supplied
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_cds2gmt (lua_State* L)
{
    const int days = (int)lua_tonumber(L, 1);      /* get argument 1 */
    const int ms = (int)lua_tonumber(L, 2);        /* get argument 2 */
    TimeLib::gmt_time_t now = TimeLib::cds2gmttime(days, ms);
    lua_pushnumber(L, now.year);                    /* push "year" as result */
    lua_pushnumber(L, now.day);                     /* push "day" as result */
    lua_pushnumber(L, now.hour);                    /* push "hour" as result */
    lua_pushnumber(L, now.minute);                  /* push "minute" as result */
    lua_pushnumber(L, now.second);                  /* push "second" as result */
    lua_pushnumber(L, now.millisecond);             /* push "millisecond" as result */
    return 6;                                       /* six results */
}

/*----------------------------------------------------------------------------
 * ltime_gmt2gps -
 *
 *  gps = time.gmt2gps(year, day of year, hour, minute, second)
 *  gps = time.gmt2gps("<year>:<month>:<day of month>:<hour in day>:<minute in hour>:<second in minute>")
 *  gps = time.gmt2gps("<year>:<day of year>:<hour in day>:<minute in hour>:<second in minute>")
 *
 *  gps: returns number of milliseconds since GPS epoch
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_gmt2gps (lua_State* L)
{
    if(lua_isnumber(L, 1)) // comma separated list
    {
        if (lua_gettop(L) != 5)
        {
            return luaL_error(L, "expecting 5 arguments");
        }
        else
        {
            TimeLib::gmt_time_t gmt;
            gmt.year = (int)lua_tonumber(L, 1);             /* get argument 1 */
            gmt.day = (int)lua_tonumber(L, 2);              /* get argument 2 */
            gmt.hour = (int)lua_tonumber(L, 3);             /* get argument 3 */
            gmt.minute = (int)lua_tonumber(L, 4);           /* get argument 4 */
            gmt.second = (int)lua_tonumber(L, 5);           /* get argument 5 */
            gmt.millisecond = (int)lua_tonumber(L, 5) - gmt.second; /* fractional part */
            int64_t ms = TimeLib::gmt2gpstime(gmt);
            lua_pushnumber(L, (lua_Number)ms);              /* push "ms" as result */
            return 1;                                       /* one result */
        }
    }
    else // string
    {
        const char* gmt = lua_tostring(L, 1);           /* get argument 1 */
        int64_t ms = TimeLib::str2gpstime(gmt);
        lua_pushnumber(L, (lua_Number)ms);              /* push "ms" as result */
        return 1;                                       /* one result */
    }
}

