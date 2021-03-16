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
RecordObject* createCcsdsRec(const char* rec_type)
{
    return new CcsdsRecord(rec_type);
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
        {"interleaver", CcsdsPacketInterleaver::luaCreate},
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
    print2term("%s package initialized (%s)\n", LUA_CCSDS_LIBNAME, BINID);
}

void deinitccsds (void)
{
}