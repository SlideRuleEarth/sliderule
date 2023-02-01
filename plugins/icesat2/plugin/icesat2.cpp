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
 *INCLUDES
 ******************************************************************************/

#include "core.h"
#include "icesat2.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_ICESAT2_LIBNAME    "icesat2"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * icesat2_version
 *----------------------------------------------------------------------------*/
int icesat2_version (lua_State* L)
{
    /* Display Version Information on Terminal */
    print2term("ICESat-2 Plugin Version: %s\n", BINID);
    print2term("Build Information: %s\n", BUILDINFO);

    /* Return Version Information to Lua */
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * icesat2_open
 *----------------------------------------------------------------------------*/
int icesat2_open (lua_State *L)
{
    static const struct luaL_Reg icesat2_functions[] = {
        {"parms",               RqstParms::luaCreate},
        {"atl03",               Atl03Reader::luaCreate},
        {"atl03indexer",        Atl03Indexer::luaCreate},
        {"atl06",               Atl06Dispatch::luaCreate},
        {"atl08",               Atl08Dispatch::luaCreate},
        {"ut_atl06",            UT_Atl06Dispatch::luaCreate},
        {"ut_atl03",            UT_Atl03Reader::luaCreate},
        {"version",             icesat2_version},
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, icesat2_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "CNF_POSSIBLE_TEP",            RqstParms::CNF_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "CNF_NOT_CONSIDERED",          RqstParms::CNF_NOT_CONSIDERED);
    LuaEngine::setAttrInt(L, "CNF_BACKGROUND",              RqstParms::CNF_BACKGROUND);
    LuaEngine::setAttrInt(L, "CNF_WITHIN_10M",              RqstParms::CNF_WITHIN_10M);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_LOW",             RqstParms::CNF_SURFACE_LOW);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_MEDIUM",          RqstParms::CNF_SURFACE_MEDIUM);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_HIGH",            RqstParms::CNF_SURFACE_HIGH);
    LuaEngine::setAttrInt(L, "QUALITY_NOMINAL",             RqstParms::QUALITY_NOMINAL);
    LuaEngine::setAttrInt(L, "QUALITY_AFTERPULSE",          RqstParms::QUALITY_POSSIBLE_AFTERPULSE);
    LuaEngine::setAttrInt(L, "QUALITY_IMPULSE_RESPONSE",    RqstParms::QUALITY_POSSIBLE_IMPULSE_RESPONSE);
    LuaEngine::setAttrInt(L, "QUALITY_POSSIBLE_TEP",        RqstParms::QUALITY_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "SRT_LAND",                    RqstParms::SRT_LAND);
    LuaEngine::setAttrInt(L, "SRT_OCEAN",                   RqstParms::SRT_OCEAN);
    LuaEngine::setAttrInt(L, "SRT_SEA_ICE",                 RqstParms::SRT_SEA_ICE);
    LuaEngine::setAttrInt(L, "SRT_LAND_ICE",                RqstParms::SRT_LAND_ICE);
    LuaEngine::setAttrInt(L, "SRT_INLAND_WATER",            RqstParms::SRT_INLAND_WATER);
    LuaEngine::setAttrInt(L, "ALL_TRACKS",                  RqstParms::ALL_TRACKS);
    LuaEngine::setAttrInt(L, "RPT_1",                       RqstParms::RPT_1);
    LuaEngine::setAttrInt(L, "RPT_2",                       RqstParms::RPT_2);
    LuaEngine::setAttrInt(L, "RPT_3",                       RqstParms::RPT_3);
    LuaEngine::setAttrInt(L, "NUM_TRACKS",                  RqstParms::NUM_TRACKS);
    LuaEngine::setAttrInt(L, "RQST_TIMEOUT",                RqstParms::DEFAULT_RQST_TIMEOUT);
    LuaEngine::setAttrInt(L, "NODE_TIMEOUT",                RqstParms::DEFAULT_NODE_TIMEOUT);
    LuaEngine::setAttrInt(L, "READ_TIMEOUT",                RqstParms::DEFAULT_READ_TIMEOUT);
    LuaEngine::setAttrInt(L, "ATL08_NOISE",                 RqstParms::ATL08_NOISE);
    LuaEngine::setAttrInt(L, "ATL08_GROUND",                RqstParms::ATL08_GROUND);
    LuaEngine::setAttrInt(L, "ATL08_CANOPY",                RqstParms::ATL08_CANOPY);
    LuaEngine::setAttrInt(L, "ATL08_TOP_OF_CANOPY",         RqstParms::ATL08_TOP_OF_CANOPY);
    LuaEngine::setAttrInt(L, "ATL08_UNCLASSIFIED",          RqstParms::ATL08_UNCLASSIFIED);
    LuaEngine::setAttrInt(L, "NATIVE",                      RqstParms::OUTPUT_FORMAT_NATIVE);
    LuaEngine::setAttrInt(L, "FEATHER",                     RqstParms::OUTPUT_FORMAT_FEATHER);
    LuaEngine::setAttrInt(L, "PARQUET",                     RqstParms::OUTPUT_FORMAT_PARQUET);
    LuaEngine::setAttrInt(L, "CVS",                         RqstParms::OUTPUT_FORMAT_CSV);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initicesat2 (void)
{
    /* Initialize Modules */
    PluginMetrics::init();
    Atl03Reader::init();
    Atl03Indexer::init();
    Atl06Dispatch::init();
    Atl08Dispatch::init();

    /* Register Cumulus IO Driver */
    Asset::registerDriver(CumulusIODriver::FORMAT, CumulusIODriver::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_ICESAT2_LIBNAME, icesat2_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_ICESAT2_LIBNAME, BINID);

    /* Display Status */
    print2term("%s plugin initialized (%s)\n", LUA_ICESAT2_LIBNAME, BINID);
}

void deiniticesat2 (void)
{
}
}
