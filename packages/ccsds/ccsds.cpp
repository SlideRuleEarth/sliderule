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

#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_CCSDS_LIBNAME   "ccsds"

/******************************************************************************
 * GLOBALS
 ******************************************************************************/

const char* CDS_KEY_CALC_NAME = "CDS";
const char* CCSDS_RECORD_CLASS = "CCSDS";
const char CCSDS_RECORD_PREFIX = '/';

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * calcCdsTime  -
 *----------------------------------------------------------------------------*/
okey_t calcCdsTime(unsigned char* buffer, int size)
{
    okey_t key = 0;
    if(buffer && size >= 12)
    {
        CcsdsSpacePacket pkt(buffer, size);
        double timestamp_ms = pkt.getCdsTime() * 1000.0;
        key = (okey_t)timestamp_ms;
    }
    return key;
}

/*----------------------------------------------------------------------------
 * createCcsdsRec  -
 *----------------------------------------------------------------------------*/
RecordObject* createCcsdsRec(const char* str)
{
    return new CcsdsRecord(str);
}

/*----------------------------------------------------------------------------
 * associateCcsdsRec  -
 *----------------------------------------------------------------------------*/
RecordObject* associateCcsdsRec(unsigned char* data, int size)
{
    return new CcsdsRecord(data, size);
}

/*----------------------------------------------------------------------------
 * ccsds_open
 *----------------------------------------------------------------------------*/
int ccsds_open (lua_State *L)
{
    static const struct luaL_Reg dispatch_functions[] = {
        {"packetizer",  CcsdsPacketizer::luaCreate},
        {"parser",      CcsdsPacketParser::luaCreate},
        {"aosmod",      CcsdsParserAOSFrameModule::luaCreate},
        {"pktmod",      CcsdsParserModule::luaCreate},
        {"stripmod",    CcsdsParserStripModule::luaCreate},
        {"zmod",        CcsdsParserZFrameModule::luaCreate},
        {"payload",     CcsdsPayloadDispatch::luaCreate},
        {"dispatcher",  CcsdsRecordDispatcher::luaCreate},
        {NULL,          NULL}
    };

    luaL_newlib(L, dispatch_functions);

    LuaEngine::setAttrInt   (L, "ALL_APIDS",    ALL_APIDS);
    LuaEngine::setAttrInt   (L, "TLM",          CcsdsPacketizer::TLM_PKT);
    LuaEngine::setAttrInt   (L, "CMD",          CcsdsPacketizer::CMD_PKT);
    LuaEngine::setAttrStr   (L, "NOSYNC",       "NOSYNC");
    LuaEngine::setAttrStr   (L, "ENCAP",        "ENCAP");
    LuaEngine::setAttrStr   (L, "SPACE",        "SPACE");

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

void initccsds (void)
{
    /* Initialize CCSDS Record Objects */
    CcsdsRecord::initCcsdsRecord();

    /* Extend Lua */
    LuaEngine::extend(LUA_CCSDS_LIBNAME, ccsds_open);

    /* Install Add On Functions */
    RecordDispatcher::addKeyCalcFunc(CDS_KEY_CALC_NAME, calcCdsTime);
    LuaLibraryMsg::lmsg_addtype(CCSDS_RECORD_CLASS, CCSDS_RECORD_PREFIX, createCcsdsRec, associateCcsdsRec);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_CCSDS_LIBNAME, BINID);

    /* Print Status */
    printf("%s package initialized (%s)\n", LUA_CCSDS_LIBNAME, BINID);
}

void deinitccsds (void)
{
}