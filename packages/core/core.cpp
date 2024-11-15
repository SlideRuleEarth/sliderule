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

#include "Asset.h"
#include "AssetIndex.h"
#include "CurlLib.h"
#include "Dictionary.h"
#include "EndpointObject.h"
#include "EndpointProxy.h"
#include "EventLib.h"
#include "PointIndex.h"
#include "FileIODriver.h"
#include "GeoDataFrame.h"
#include "HttpServer.h"
#include "List.h"
#include "LuaEndpoint.h"
#include "LuaEngine.h"
#include "LuaLibraryMsg.h"
#include "LuaLibrarySys.h"
#include "LuaLibraryTime.h"
#include "LuaObject.h"
#include "LuaScript.h"
#include "MathLib.h"
#include "MetricMonitor.h"
#include "Monitor.h"
#include "MsgQ.h"
#include "OrchestratorLib.h"
#include "Ordering.h"
#include "ProvisioningSystemLib.h"
#include "PublishMonitor.h"
#include "RecordObject.h"
#include "RegionMask.h"
#include "RequestFields.h"
#include "RequestMetrics.h"
#include "SpatialIndex.h"
#include "StringLib.h"
#include "Table.h"
#include "IntervalIndex.h"
#include "TimeLib.h"
#include "OsApi.h"
#ifdef __unittesting__
#include "UT_Dictionary.h"
#include "UT_Field.h"
#include "UT_List.h"
#include "UT_MsgQ.h"
#include "UT_Ordering.h"
#include "UT_String.h"
#include "UT_Table.h"
#include "UT_TimeLib.h"
#endif

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_CORE_LIBNAME    "core"

/******************************************************************************
 * LOCAL DATA
 ******************************************************************************/

bool appActive  = true;
int  appErrors  = 0;

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * os_print
 *
 *  Notes: OS API "dlog" print function
 *----------------------------------------------------------------------------*/
static void os_print (const char* file_name, unsigned int line_number, const char* message)
{
    EventLib::logMsg(file_name, line_number, CRITICAL, "%s", message);
}

/*----------------------------------------------------------------------------
 * core_open
 *----------------------------------------------------------------------------*/
static int core_open (lua_State *L)
{
    static const struct luaL_Reg core_functions[] = {
        {"getbyname",       LuaObject::luaGetByName},
        {"script",          LuaScript::luaCreate},
        {"monitor",         Monitor::luaCreate},
        {"httpd",           HttpServer::luaCreate},
        {"endpoint",        LuaEndpoint::luaCreate},
        {"asset",           Asset::luaCreate},
        {"pointindex",      PointIndex::luaCreate},
        {"intervalindex",   IntervalIndex::luaCreate},
        {"spatialindex",    SpatialIndex::luaCreate},
        {"get",             CurlLib::luaGet},
        {"put",             CurlLib::luaPut},
        {"post",            CurlLib::luaPost},
        {"proxy",           EndpointProxy::luaCreate},
        {"orchurl",         OrchestratorLib::luaUrl},
        {"orchreg",         OrchestratorLib::luaRegisterService},
        {"orchselflock",    OrchestratorLib::luaSelfLock},
        {"orchlock",        OrchestratorLib::luaLock},
        {"orchunlock",      OrchestratorLib::luaUnlock},
        {"orchhealth",      OrchestratorLib::luaHealth},
        {"orchnodes",       OrchestratorLib::luaGetNodes},
        {"psurl",           ProvisioningSystemLib::luaUrl},
        {"psorg",           ProvisioningSystemLib::luaSetOrganization},
        {"pslogin",         ProvisioningSystemLib::luaLogin},
        {"psvalidate",      ProvisioningSystemLib::luaValidate},
        {"psauth",          ProvisioningSystemLib::Authenticator::luaCreate},
        {"pmonitor",        PublishMonitor::luaCreate},
        {"mmonitor",        MetricMonitor::luaCreate},
        {"parms",           RequestFields::luaCreate},
#ifdef __unittesting__
        {"ut_dictionary",   UT_Dictionary::luaCreate},
        {"ut_field",        UT_Field::luaCreate},
        {"ut_list",         UT_List::luaCreate},
        {"ut_msgq",         UT_MsgQ::luaCreate},
        {"ut_ordering",     UT_Ordering::luaCreate},
        {"ut_string",       UT_String::luaCreate},
        {"ut_table",        UT_Table::luaCreate},
        {"ut_timelib",      UT_TimeLib::luaCreate},
#endif
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, core_functions);

    /* Set Globals */
    LuaEngine::setAttrInt   (L, "DEBUG",                    DEBUG);
    LuaEngine::setAttrInt   (L, "INFO",                     INFO);
    LuaEngine::setAttrInt   (L, "WARNING",                  WARNING);
    LuaEngine::setAttrInt   (L, "ERROR",                    ERROR);
    LuaEngine::setAttrInt   (L, "CRITICAL",                 CRITICAL);
    LuaEngine::setAttrInt   (L, "LOG",                      EventLib::LOG);
    LuaEngine::setAttrInt   (L, "TRACE",                    EventLib::TRACE);
    LuaEngine::setAttrInt   (L, "METRIC",                   EventLib::METRIC);
    LuaEngine::setAttrInt   (L, "FMT_TEXT",                 Monitor::TEXT);
    LuaEngine::setAttrInt   (L, "FMT_JSON",                 Monitor::JSON);
    LuaEngine::setAttrInt   (L, "FMT_CLOUD",                Monitor::CLOUD);
    LuaEngine::setAttrInt   (L, "FMT_RECORD",               Monitor::RECORD);
    LuaEngine::setAttrStr   (L, "EVENTQ",                   EVENTQ);
    LuaEngine::setAttrInt   (L, "STRING",                   RecordObject::TEXT);
    LuaEngine::setAttrInt   (L, "REAL",                     RecordObject::REAL);
    LuaEngine::setAttrInt   (L, "INTEGER",                  RecordObject::INTEGER);
    LuaEngine::setAttrInt   (L, "DYNAMIC",                  RecordObject::DYNAMIC);
    LuaEngine::setAttrInt   (L, "NORTH_POLAR",              MathLib::NORTH_POLAR);
    LuaEngine::setAttrInt   (L, "SOUTH_POLAR",              MathLib::SOUTH_POLAR);
    LuaEngine::setAttrInt   (L, "PEND",                     IO_PEND);
    LuaEngine::setAttrInt   (L, "CHECK",                    IO_CHECK);
    LuaEngine::setAttrInt   (L, "SUBSCRIBER_OF_OPPORUNITY", MsgQ::SUBSCRIBER_OF_OPPORTUNITY);
    LuaEngine::setAttrInt   (L, "SUBSCRIBER_OF_CONFIDENCE", MsgQ::SUBSCRIBER_OF_CONFIDENCE);
    LuaEngine::setAttrInt   (L, "RTE_INFO",                 RTE_INFO);
    LuaEngine::setAttrInt   (L, "RTE_ERROR",                RTE_ERROR);
    LuaEngine::setAttrInt   (L, "RTE_TIMEOUT",              RTE_TIMEOUT);
    LuaEngine::setAttrInt   (L, "RTE_RESOURCE_DOES_NOT_EXIST",  RTE_RESOURCE_DOES_NOT_EXIST);
    LuaEngine::setAttrInt   (L, "RTE_EMPTY_SUBSET",         RTE_EMPTY_SUBSET);
    LuaEngine::setAttrInt   (L, "RTE_SIMPLIFY",             RTE_SIMPLIFY);
    LuaEngine::setAttrInt   (L, "INT8",                     RecordObject::INT8);
    LuaEngine::setAttrInt   (L, "INT16",                    RecordObject::INT16);
    LuaEngine::setAttrInt   (L, "INT32",                    RecordObject::INT32);
    LuaEngine::setAttrInt   (L, "INT64",                    RecordObject::INT64);
    LuaEngine::setAttrInt   (L, "UINT8",                    RecordObject::UINT8);
    LuaEngine::setAttrInt   (L, "UINT16",                   RecordObject::UINT16);
    LuaEngine::setAttrInt   (L, "UINT32",                   RecordObject::UINT32);
    LuaEngine::setAttrInt   (L, "UINT64",                   RecordObject::UINT64);
    LuaEngine::setAttrInt   (L, "BITFIELD",                 RecordObject::BITFIELD);
    LuaEngine::setAttrInt   (L, "FLOAT",                    RecordObject::FLOAT);
    LuaEngine::setAttrInt   (L, "DOUBLE",                   RecordObject::DOUBLE);
    LuaEngine::setAttrInt   (L, "TIME8",                    RecordObject::TIME8);
    LuaEngine::setAttrInt   (L, "STRING",                   RecordObject::STRING);
    LuaEngine::setAttrInt   (L, "USER",                     RecordObject::USER);
    LuaEngine::setAttrInt   (L, "RQST_TIMEOUT",             RequestFields::DEFAULT_TIMEOUT);
    LuaEngine::setAttrInt   (L, "NODE_TIMEOUT",             RequestFields::DEFAULT_TIMEOUT);
    LuaEngine::setAttrInt   (L, "READ_TIMEOUT",             RequestFields::DEFAULT_TIMEOUT);
    LuaEngine::setAttrInt   (L, "CLUSTER_SIZE_HINT",        0);
    LuaEngine::setAttrInt   (L, "MAX_LOCKS_PER_NODE",       OrchestratorLib::MAX_LOCKS_PER_NODE);
    LuaEngine::setAttrInt   (L, "INVALID_TX_ID",            OrchestratorLib::INVALID_TX_ID);
    LuaEngine::setAttrStr   (L, "TERMINATE",                GeoDataFrame::TERMINATE);

#ifdef __unittesting__
    LuaEngine::setAttrBool(L, "UNITTEST",                   true);
#else
    LuaEngine::setAttrBool(L, "UNITTEST",                   false);
#endif

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
    /* Initialize Platform */
    OsApi::init(os_print);

    /* Initialize Libraries */
    EventLib::init(EVENTQ);  /* Must be called first to handle events (mlog msgs) */
    MsgQ::init();
    SockLib::init();
    TTYLib::init();
    TimeLib::init();
    LuaEngine::init();
    GeoDataFrame::init();
    RequestMetrics::init();
    CurlLib::init();
    OrchestratorLib::init();
    ProvisioningSystemLib::init();
#ifdef __unittesting__
    UT_TimeLib::init();
#endif

    /* Register IO Drivers */
    Asset::registerDriver("nil", Asset::IODriver::create);
    Asset::registerDriver(FileIODriver::FORMAT, FileIODriver::create);

    /* Initialize Modules */
    LuaEndpoint::init();

    /* Initialize Lua Extensions */
    LuaLibrarySys::lsys_init();
    LuaLibraryMsg::lmsg_init();
    LuaLibraryTime::ltime_init();

    /* Add Lua Extensions */
    LuaEngine::extend(LuaLibraryMsg::LUA_MSGLIBNAME, LuaLibraryMsg::luaopen_msglib);
    LuaEngine::extend(LuaLibrarySys::LUA_SYSLIBNAME, LuaLibrarySys::luaopen_syslib);
    LuaEngine::extend(LuaLibraryTime::LUA_TIMELIBNAME, LuaLibraryTime::luaopen_timelib);
    LuaEngine::extend(LUA_CORE_LIBNAME, core_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_CORE_LIBNAME, LIBID);

    /* Print Status */
    print2term("%s package initialized (%s)\n", LUA_CORE_LIBNAME, LIBID);
}

/*----------------------------------------------------------------------------
 * deinitcore
 *
 *  uninitialize core package
 *----------------------------------------------------------------------------*/
void deinitcore (void)
{
    print2term("Exiting... ");
    ProvisioningSystemLib::deinit();
    OrchestratorLib::deinit();
    CurlLib::deinit();
    LuaEngine::deinit();
    EventLib::deinit();
    TimeLib::deinit();
    TTYLib::deinit();
    SockLib::deinit();
    MsgQ::deinit();
    OsApi::deinit();
    print2term("cleanup complete (%d errors)\n", appErrors);
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
void setinactive(int errors)
{
    appErrors = errors;
    appActive = false;
}

/*----------------------------------------------------------------------------
 *  geterrors
 *
 *  get aplication errors
 *----------------------------------------------------------------------------*/
int geterrors(void)
{
    return appErrors;
}

