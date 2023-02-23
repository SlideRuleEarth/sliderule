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

#include "LuaLibraryTime.h"
#include "LuaEngine.h"
#include "core.h"

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
    {"gps2date", LuaLibraryTime::ltime_gps2date},
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
    int64_t now = TimeLib::gpstime();
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
    TimeLib::gmt_time_t now = TimeLib::gmttime();
    lua_pushnumber(L, now.year);                    /* push "year" as result */
    lua_pushnumber(L, now.doy);                     /* push "day of year" as result */
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
    lua_pushnumber(L, now.doy);                     /* push "day of year" as result */
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
    lua_pushnumber(L, now.doy);                     /* push "day of year" as result */
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
            gmt.doy = (int)lua_tonumber(L, 2);              /* get argument 2 */
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
        if(!gmt) return luaL_error(L, "invalid string passed to gmt2gps function");

        int64_t ms = TimeLib::str2gpstime(gmt);
        lua_pushnumber(L, (lua_Number)ms);              /* push "ms" as result */
        return 1;                                       /* one result */
    }
}

/*----------------------------------------------------------------------------
 * ltime_gps2date - year, month, day, hour, minute, second, millisecond = time.gps2date(gps)
 *
 *  gps: number of milliseconds since GPS epoch
 *
 *  returns list specifying date corresponding to the gps time supplied
 *----------------------------------------------------------------------------*/
int LuaLibraryTime::ltime_gps2date (lua_State* L)
{
    const int64_t gpsms = (int64_t)lua_tonumber(L, 1);     /* get argument 1 */
    TimeLib::gmt_time_t now = TimeLib::gps2gmttime(gpsms);
    TimeLib::date_t date = TimeLib::gmt2date(now);
    lua_pushinteger(L, now.year);                   /* push "year" as result */
    lua_pushinteger(L, date.month);                 /* push "month" as result */
    lua_pushinteger(L, date.day);                   /* push "day" as result */
    lua_pushinteger(L, now.hour);                   /* push "hour" as result */
    lua_pushinteger(L, now.minute);                 /* push "minute" as result */
    lua_pushinteger(L, now.second);                 /* push "second" as result */
    lua_pushinteger(L, now.millisecond);            /* push "millisecond" as result */
    return 7;                                       /* six results */
}
