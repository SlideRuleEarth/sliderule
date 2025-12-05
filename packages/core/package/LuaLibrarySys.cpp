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

#include <unistd.h>
#include <filesystem>

#include "LuaLibrarySys.h"
#include "LuaEngine.h"
#include "SystemConfig.h"
#include "OsApi.h"
#include "TimeLib.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "DeviceObject.h"
#include "core.h"

namespace fs = std::filesystem;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* LuaLibrarySys::LUA_SYSLIBNAME = "sys";
const struct luaL_Reg LuaLibrarySys::sysLibs [] = {
    {"version",     LuaLibrarySys::lsys_version},
    {"quit",        LuaLibrarySys::lsys_quit},
    {"abort",       LuaLibrarySys::lsys_abort},
    {"alive",       LuaLibrarySys::lsys_alive},
    {"wait",        LuaLibrarySys::lsys_wait},
    {"log",         LuaLibrarySys::lsys_log},
    {"metric",      LuaLibrarySys::lsys_metric},
    {"lsmsgq",      LuaLibrarySys::lsys_lsmsgq},
    {"type",        LuaLibrarySys::lsys_type},
    {"setiosz",     LuaLibrarySys::lsys_setiosize},
    {"getiosz",     LuaLibrarySys::lsys_getiosize},
    {"healthy",     LuaLibrarySys::lsys_healthy},
    {"lsrec",       LuaLibrarySys::lsys_lsrec},
    {"lsobj",       LuaLibrarySys::lsys_lsobj},
    {"cwd",         LuaLibrarySys::lsys_cwd},
    {"pathfind",    LuaLibrarySys::lsys_pathfind},
    {"filefind",    LuaLibrarySys::lsys_filefind},
    {"fileexists",  LuaLibrarySys::lsys_fileexists},
    {"deletefile",  LuaLibrarySys::lsys_deletefile},
    {"memu",        LuaLibrarySys::lsys_memu},
    {"upleap",      LuaLibrarySys::lsys_updateleapsecs},
    {"lsdev",       DeviceObject::luaList},
    {"initcfg",     SystemConfig::luaPopulate},
    {"getcfg",      SystemConfig::luaGetField},
    {"setcfg",      SystemConfig::luaSetField},
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
    bool verbose = false;
    if(lua_isboolean(L, 1))
    {
        verbose = lua_toboolean(L, 1);
    }

    /* Get Information */
    const int64_t launch_time_gps = TimeLib::sys2gpstime(OsApi::getLaunchTime());
    const TimeLib::gmt_time_t timeinfo = TimeLib::gps2gmttime(launch_time_gps);
    const TimeLib::date_t dateinfo = TimeLib::gmt2date(timeinfo);
    const FString timestr("%04d-%02d-%02dT%02d:%02d:%02dZ", timeinfo.year, dateinfo.month, dateinfo.day, timeinfo.hour, timeinfo.minute, timeinfo.second);
    const int64_t duration = TimeLib::gpstime() - launch_time_gps;
    const char** pkg_list = LuaEngine::getPkgList();

    /* Display Information on Terminal */
    if(verbose)
    {
        print2term("SlideRule Version:   %s\n", LIBID);
        print2term("Build Information:   %s\n", BUILDINFO);
        print2term("Launch Time: %s\n", timestr.c_str());
        print2term("Duration: %.2lf days\n", (double)duration / 1000.0 / 60.0 / 60.0 / 24.0); // milliseconds / seconds / minutes / hours
        print2term("Packages: [ ");
        if(pkg_list)
        {
            int index = 0;
            while(pkg_list[index])
            {
                print2term("%s", pkg_list[index]);
                index++;
                if(pkg_list[index]) print2term(", ");
            }
        }
        print2term(" ]\n");
    }

    /* Return Information to Lua (and clean up package list) */
    lua_pushstring(L, LIBID);
    lua_pushstring(L, BUILDINFO);
    lua_pushstring(L, timestr.c_str());
    lua_pushinteger(L, duration);
    lua_newtable(L);
    if(pkg_list)
    {
        int index = 0;
        while(pkg_list[index])
        {
            lua_pushstring(L, pkg_list[index]);
            lua_rawseti(L, -2, index+1);
            delete [] pkg_list[index];
            index++;
        }
        delete [] pkg_list;
    }

    return 5;
}

/*----------------------------------------------------------------------------
 * lsys_quit
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_quit (lua_State* L)
{
    /* Get errors reported by lua app */
    int errors = 0;
    if(lua_isnumber(L, 1))
    {
        errors = lua_tointeger(L, 1);
    }

    setinactive( errors );

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
 * lsys_alive
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_alive (lua_State* L)
{
    lua_pushboolean(L, checkactive());
    return 1;
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
        secs = lua_tointeger(L, 1);
    }
    else
    {
        mlog(CRITICAL, "Incorrect parameter type for seconds to wait");
        lua_pushboolean(L, false); /* push result as fail */
        return 1;
    }

    /* Wait */
    OsApi::sleep(secs);

    /* Return Success */
    lua_pushboolean(L, true);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_log - .log(<level>, <message>)
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_log (lua_State* L)
{
    if(lua_isinteger(L, 1))
    {
        const event_level_t lvl = (event_level_t)lua_tointeger(L, 1);
        if(lua_isstring(L, 2))
        {
            mlog(lvl, "%s", lua_tostring(L, 2));
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * lsys_metric - .metric()
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_metric (lua_State* L)
{
    lua_newtable(L);

    /* Alive */
    lua_pushstring(L, "alive");
    lua_newtable(L);
    {
        lua_pushstring(L, "value");
        lua_pushnumber(L, 1);
        lua_settable(L, -3);

        lua_pushstring(L, "type");
        lua_pushstring(L, "GAUGE");
        lua_settable(L, -3);
    }
    lua_settable(L, -3);

    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_lsmsgq
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_lsmsgq (lua_State* L)
{
    (void)L;

    const int num_msgqs = MsgQ::numQ();
    if(num_msgqs > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[num_msgqs];
        const int numq = MsgQ::listQ(msgQs, num_msgqs);
        print2term("\n");
        for(int i = 0; i < numq; i++)
        {
            print2term("MSGQ: %40s %8d %9s %d\n",
                msgQs[i].name, msgQs[i].len, msgQs[i].state,
                msgQs[i].subscriptions);
        }
        print2term("\n");
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
 * lsys_setiosize
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_setiosize (lua_State* L)
{
    bool status = false;

    if(!lua_isnumber(L, 1))
    {
        mlog(CRITICAL, "I/O maximum size must be a number");
    }
    else
    {
        /* Set I/O Size */
        const int size = lua_tointeger(L, 1);
        status = OsApi::setIOMaxsize(size);
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
    lua_pushnumber(L, OsApi::getIOMaxsize());
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_healthy
 *  - this is currently a placeholder for a more sophisticated health check
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_healthy (lua_State* L)
{
    bool health = true;

    /* Check Memory Usage */
    const double current_memory_usage = OsApi::memusage();
    if(current_memory_usage >= SystemConfig::settings().streamMemoryThreshold.value)
    {
        health = false;
    }

    /* Return Health */
    lua_pushboolean(L, health);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_lsrec
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_lsrec (lua_State* L)
{
    const char* pattern = NULL;
    if(lua_isstring(L, 1)) pattern = lua_tostring(L, 1);

    print2term("\n%50s %24s %s\n", "Type", "Id", "Size");
    char** rectypes = NULL;
    const int numrecs = RecordObject::getRecords(&rectypes);
    for(int i = 0; i < numrecs; i++)
    {
        if(pattern == NULL || StringLib::find(rectypes[i], pattern))
        {
            const char* id_field = RecordObject::getRecordIdField(rectypes[i]);
            const int data_size = RecordObject::getRecordDataSize(rectypes[i]);
            print2term("%50s %24s %d\n", rectypes[i], id_field != NULL ? id_field : "NA", data_size);
        }
        delete [] rectypes[i];
    }
    delete [] rectypes;
    return 0;
}

/*----------------------------------------------------------------------------
 * lsys_lsobj
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_lsobj (lua_State* L)
{
    (void)L;

    vector<LuaObject::object_info_t> globals;
    LuaObject::getGlobalObjects(globals);
    print2term("\n%30s   %s\n", "Object Name", "Reference");
    for(const LuaObject::object_info_t& object: globals)
    {
        const long reference_count = object.refCnt;
        print2term("%30s   %ld        %s\n", object.objName.c_str(), reference_count, object.objType.c_str());
    }

    print2term("\nNumber of Global Objects: %lu\n", (unsigned long)globals.size());
    print2term("Total Number of Objects: %ld\n", LuaObject::getNumObjects());

    return 0;
}

/*----------------------------------------------------------------------------
 * lsys_cwd - current working directory
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_cwd (lua_State* L)
{
    char cwd[MAX_STR_SIZE];
    const char* cwd_ptr = getcwd(cwd, MAX_STR_SIZE);

    if(cwd_ptr != NULL)
    {
        lua_pushstring(L, cwd);
        return 1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * lsys_pathfind - recursively search directory looking for dir_name
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_pathfind (lua_State* L)
{
    const char* search_dir = ".";
    if(lua_isstring(L, 1)) search_dir = lua_tostring(L, 1);
    const string base_dir = search_dir;

    const char* match_dir = NULL;
    string target_dir;
    if(lua_isstring(L, 2))
    {
        match_dir = lua_tostring(L, 2);
        target_dir = match_dir;
    }

    int r = 1;
    lua_newtable(L);
    for(const auto& entry: fs::recursive_directory_iterator(base_dir))
    {
        if(entry.is_directory())
        {
            if(!match_dir || (entry.path().filename() == target_dir))
            {
                lua_pushstring(L, entry.path().c_str());
                lua_rawseti(L, -2, r++);
            }
        }
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_filefind - search directory looking for file_name
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_filefind (lua_State* L)
{
    const char* search_dir = ".";
    if(lua_isstring(L, 1)) search_dir = lua_tostring(L, 1);
    const string base_dir = search_dir;

    const char* match_extension = NULL;
    string target_extension;
    if(lua_isstring(L, 2))
    {
        match_extension = lua_tostring(L, 2);
        target_extension = match_extension;
    }

    int r = 1;
    lua_newtable(L);
    for (const auto& entry: fs::directory_iterator(base_dir))
    {
        if (entry.is_regular_file())
        {
            if(!match_extension || (entry.path().extension() == target_extension))
            {
                lua_pushstring(L, entry.path().c_str());
                lua_rawseti(L, -2, r++);
            }
        }
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_fileexists - check if a file exists
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_fileexists (lua_State* L)
{
    if(lua_isstring(L, 1))
    {
        const char* filename = lua_tostring(L, 1);
        lua_pushboolean(L, fs::exists(filename));
    }
    else
    {
        lua_pushboolean(L, false);
    }
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_deletefile - deletes file it it exists
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_deletefile (lua_State* L)
{
    bool status = false;
    if(lua_isstring(L, 1))
    {
        const char* filename = lua_tostring(L, 1);
        if(fs::exists(filename))
        {
            const int rc = std::remove(filename);
            if(rc == 0)
            {
                status = true;
            }
            else
            {
                char err_buf[256];
                mlog(CRITICAL, "Failed (%d) to delete file %s: %s", rc, filename, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
            }
        }
    }
    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_memu - memory usage
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_memu (lua_State* L)
{
    const double m = OsApi::memusage();
    lua_pushnumber(L, m);
    return 1;
}

/*----------------------------------------------------------------------------
 * lsys_updateleapsecs - update leap seconds
 *----------------------------------------------------------------------------*/
int LuaLibrarySys::lsys_updateleapsecs (lua_State* L)
{
    bool status = false;
    if(lua_isstring(L, 1))
    {
        const char* filename = lua_tostring(L, 1);
        if(fs::exists(filename))
        {
            status = TimeLib::parsenistfile(filename);
        }
    }
    lua_pushboolean(L, status);
    return 1;
}
