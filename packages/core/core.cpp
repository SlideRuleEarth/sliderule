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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_CORE_LIBNAME    "core"

/******************************************************************************
 * LOCAL DATA
 ******************************************************************************/

bool appActive = true;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * os_print
 *
 *  Notes: OS API "dlog" print function
 *----------------------------------------------------------------------------*/
void os_print (const char* file_name, unsigned int line_number, const char* message)
{
    LogLib::logMsg(file_name, line_number, CRITICAL, "%s", message);
}

/*----------------------------------------------------------------------------
 * core_open
 *----------------------------------------------------------------------------*/
int core_open (lua_State *L)
{
    static const struct luaL_Reg core_functions[] = {
        {"logger",      Logger::luaCreate},
        {"cluster",     ClusterSocket::luaCreate},
        {"file",        File::luaCreate},
        {"tcp",         TcpSocket::luaCreate},
        {"uart",        Uart::luaCreate},
        {"udp",         UdpSocket::luaCreate},
        {"reader",      DeviceReader::luaCreate},
        {"writer",      DeviceWriter::luaCreate},
        {"dispatcher",  RecordDispatcher::luaCreate},
        {"capture",     CaptureDispatch::luaCreate},
        {"limit",       LimitDispatch::luaCreate},
        {"metric",      MetricDispatch::luaCreate},
        {"publish",     PublisherDispatch::luaCreate},
        {"report",      ReportDispatch::luaCreate},
        {NULL,          NULL}
    };

    /* Set Library */
    luaL_newlib(L, core_functions);

    /* Set Globals */
    LuaEngine::setAttrInt   (L, "IGNORE",               IGNORE);
    LuaEngine::setAttrInt   (L, "DEBUG",                DEBUG);
    LuaEngine::setAttrInt   (L, "INFO",                 INFO);
    LuaEngine::setAttrInt   (L, "WARNING",              WARNING);
    LuaEngine::setAttrInt   (L, "ERROR",                ERROR);
    LuaEngine::setAttrInt   (L, "CRITICAL",             CRITICAL);
    LuaEngine::setAttrInt   (L, "RAW",                  RAW);
    LuaEngine::setAttrInt   (L, "STRING",               RecordObject::TEXT);
    LuaEngine::setAttrInt   (L, "REAL",                 RecordObject::REAL);
    LuaEngine::setAttrInt   (L, "INTEGER",              RecordObject::INTEGER);
    LuaEngine::setAttrInt   (L, "DYNAMIC",              RecordObject::INVALID_VALUE);
    LuaEngine::setAttrInt   (L, "READER",               DeviceObject::READER);
    LuaEngine::setAttrInt   (L, "WRITER",               DeviceObject::WRITER);
    LuaEngine::setAttrInt   (L, "DUPLEX",               DeviceObject::DUPLEX);
    LuaEngine::setAttrBool  (L, "SERVER",               true);
    LuaEngine::setAttrBool  (L, "CLIENT",               false);
    LuaEngine::setAttrInt   (L, "DIE_ON_DISCONNECT",    true);
    LuaEngine::setAttrInt   (L, "PERSISTENT",           false);
    LuaEngine::setAttrInt   (L, "BLOCK",                true);
    LuaEngine::setAttrInt   (L, "QUEUE",                ClusterSocket::QUEUE);
    LuaEngine::setAttrInt   (L, "BUS",                  ClusterSocket::BUS);
    LuaEngine::setAttrInt   (L, "BINARY",               File::BINARY);
    LuaEngine::setAttrInt   (L, "ASCII",                File::ASCII);
    LuaEngine::setAttrInt   (L, "TEXT",                 File::TEXT);
    LuaEngine::setAttrInt   (L, "FIFO",                 File::FIFO);
    LuaEngine::setAttrInt   (L, "FLUSHED",              File::FLUSHED);
    LuaEngine::setAttrInt   (L, "CACHED",               File::CACHED);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 *  initcore
 *
 *  initialize core package
 *----------------------------------------------------------------------------*/
void initcore (void)
{
    /* Initialize Libraries */
    LocalLib::init();
    SockLib::init();
    TTYLib::init();
    TimeLib::init();
    TraceLib::init();
    LogLib::init();
    MsgQ::init();

    /* Attach OsApi Print Function */
    LocalLib::setPrint(os_print);

    /* Initialize Default Lua Extensions */
    LuaLibrarySys::lsys_init();
    LuaLibraryMsg::lmsg_init();
    LuaLibraryTime::ltime_init();

    /* Add Default Lua Extensions */
    LuaEngine::extend(LuaLibraryMsg::LUA_MSGLIBNAME, LuaLibraryMsg::luaopen_msglib);
    LuaEngine::extend(LuaLibrarySys::LUA_SYSLIBNAME, LuaLibrarySys::luaopen_syslib);
    LuaEngine::extend(LuaLibraryTime::LUA_TIMELIBNAME, LuaLibraryTime::luaopen_timelib);
    LuaEngine::extend(LUA_CORE_LIBNAME, core_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_CORE_LIBNAME, BINID);

    /* Print Status */
    printf("%s package initialized (%s)\n", LUA_CORE_LIBNAME, BINID);
}

/*----------------------------------------------------------------------------
 * deinitcore
 *
 *  uninitialize core package
 *----------------------------------------------------------------------------*/
void deinitcore (void)
{
    /* Clean up libraries initialized in initcore() */
    printf("Exiting...\n");
    LuaObject::releaseLockedLuaObjects();
    MsgQ::deinit();     printf("Message Queues Uninitialized\n");
    LogLib::deinit();   printf("Logging Library Uninitialized\n");
    TraceLib::deinit(); printf("Tracing Library Unitialized\n");
    TimeLib::deinit();  printf("Time Library Uninitialized\n");
    TTYLib::deinit();   printf("TTY Library Uninitialized\n");
    SockLib::deinit();  printf("Socket Library Uninitialized\n");
    LocalLib::deinit(); printf("Local Library Uninitialized\n");
    printf("Cleanup Complete\n");
}

/*----------------------------------------------------------------------------
 * checkactive
 *
 *  check application active
 *----------------------------------------------------------------------------*/
bool checkactive(void)
{
    return appActive;
}

/*----------------------------------------------------------------------------
 * setinactive
 *
 *  set application active
 *----------------------------------------------------------------------------*/
void setinactive(void)
{
    appActive = false;
}
