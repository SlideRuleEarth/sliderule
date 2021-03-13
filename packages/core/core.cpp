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

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_CORE_LIBNAME    "core"

#ifndef MONITORQ
#define MONITORQ "monitorq"
#endif

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
    EventLib::logMsg(file_name, line_number, CRITICAL, "%s", message);
}

/*----------------------------------------------------------------------------
 * core_open
 *----------------------------------------------------------------------------*/
int core_open (lua_State *L)
{
    static const struct luaL_Reg core_functions[] = {
        {"getbyname",       LuaObject::luaGetByName},
        {"monitor",         Monitor::luaCreate},
        {"cluster",         ClusterSocket::luaCreate},
        {"file",            File::luaCreate},
        {"tcp",             TcpSocket::luaCreate},
        {"uart",            Uart::luaCreate},
        {"udp",             UdpSocket::luaCreate},
        {"reader",          DeviceReader::luaCreate},
        {"writer",          DeviceWriter::luaCreate},
        {"httpd",           HttpServer::luaCreate},
        {"endpoint",        LuaEndpoint::luaCreate},
        {"dispatcher",      RecordDispatcher::luaCreate},
        {"capture",         CaptureDispatch::luaCreate},
        {"limit",           LimitDispatch::luaCreate},
        {"metric",          MetricDispatch::luaCreate},
        {"publish",         PublisherDispatch::luaCreate},
        {"report",          ReportDispatch::luaCreate},
        {"csv",             CsvDispatch::luaCreate},
        {"bridge",          MsgBridge::luaCreate},
        {"asset",           Asset::luaCreate},
        {"pointindex",      PointIndex::luaCreate},
        {"intervalindex",   IntervalIndex::luaCreate},
        {"spatialindex",    SpatialIndex::luaCreate},
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, core_functions);

    /* Set Globals */
    LuaEngine::setAttrInt   (L, "DEBUG",                DEBUG);
    LuaEngine::setAttrInt   (L, "INFO",                 INFO);
    LuaEngine::setAttrInt   (L, "WARNING",              WARNING);
    LuaEngine::setAttrInt   (L, "ERROR",                ERROR);
    LuaEngine::setAttrInt   (L, "CRITICAL",             CRITICAL);
    LuaEngine::setAttrInt   (L, "RAW",                  RAW);
    LuaEngine::setAttrInt   (L, "LOG",                  EventLib::LOG);
    LuaEngine::setAttrInt   (L, "TRACE",                EventLib::TRACE);
    LuaEngine::setAttrInt   (L, "METRIC",               EventLib::METRIC);
    LuaEngine::setAttrInt   (L, "FMT_TEXT",             Monitor::TEXT);
    LuaEngine::setAttrInt   (L, "FMT_JSON",             Monitor::JSON);
    LuaEngine::setAttrStr   (L, "MONITORQ",             MONITORQ);
    LuaEngine::setAttrInt   (L, "STRING",               RecordObject::TEXT);
    LuaEngine::setAttrInt   (L, "REAL",                 RecordObject::REAL);
    LuaEngine::setAttrInt   (L, "INTEGER",              RecordObject::INTEGER);
    LuaEngine::setAttrInt   (L, "DYNAMIC",              RecordObject::DYNAMIC);
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
    LuaEngine::setAttrInt   (L, "NORTH_POLAR",          MathLib::NORTH_POLAR);    
    LuaEngine::setAttrInt   (L, "SOUTH_POLAR",          MathLib::SOUTH_POLAR);    
    LuaEngine::setAttrInt   (L, "PEND",                 IO_PEND);
    LuaEngine::setAttrInt   (L, "CHECK",                IO_CHECK);

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
    MsgQ::init();
    SockLib::init();
    TTYLib::init();
    TimeLib::init();
    EventLib::init(MONITORQ);

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
    EventLib::deinit(); printf("Event Library Uninitialized\n");
    TimeLib::deinit();  printf("Time Library Uninitialized\n");
    TTYLib::deinit();   printf("TTY Library Uninitialized\n");
    SockLib::deinit();  printf("Socket Library Uninitialized\n");
    MsgQ::deinit();     printf("Message Queues Uninitialized\n");
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
