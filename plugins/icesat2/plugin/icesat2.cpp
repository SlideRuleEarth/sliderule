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
        {"parms",               Icesat2Parms::luaCreate},
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
    LuaEngine::setAttrInt(L, "CNF_POSSIBLE_TEP",            Icesat2Parms::CNF_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "CNF_NOT_CONSIDERED",          Icesat2Parms::CNF_NOT_CONSIDERED);
    LuaEngine::setAttrInt(L, "CNF_BACKGROUND",              Icesat2Parms::CNF_BACKGROUND);
    LuaEngine::setAttrInt(L, "CNF_WITHIN_10M",              Icesat2Parms::CNF_WITHIN_10M);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_LOW",             Icesat2Parms::CNF_SURFACE_LOW);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_MEDIUM",          Icesat2Parms::CNF_SURFACE_MEDIUM);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_HIGH",            Icesat2Parms::CNF_SURFACE_HIGH);
    LuaEngine::setAttrInt(L, "QUALITY_NOMINAL",             Icesat2Parms::QUALITY_NOMINAL);
    LuaEngine::setAttrInt(L, "QUALITY_AFTERPULSE",          Icesat2Parms::QUALITY_POSSIBLE_AFTERPULSE);
    LuaEngine::setAttrInt(L, "QUALITY_IMPULSE_RESPONSE",    Icesat2Parms::QUALITY_POSSIBLE_IMPULSE_RESPONSE);
    LuaEngine::setAttrInt(L, "QUALITY_POSSIBLE_TEP",        Icesat2Parms::QUALITY_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "SRT_LAND",                    Icesat2Parms::SRT_LAND);
    LuaEngine::setAttrInt(L, "SRT_OCEAN",                   Icesat2Parms::SRT_OCEAN);
    LuaEngine::setAttrInt(L, "SRT_SEA_ICE",                 Icesat2Parms::SRT_SEA_ICE);
    LuaEngine::setAttrInt(L, "SRT_LAND_ICE",                Icesat2Parms::SRT_LAND_ICE);
    LuaEngine::setAttrInt(L, "SRT_INLAND_WATER",            Icesat2Parms::SRT_INLAND_WATER);
    LuaEngine::setAttrInt(L, "ALL_TRACKS",                  Icesat2Parms::ALL_TRACKS);
    LuaEngine::setAttrInt(L, "RPT_1",                       Icesat2Parms::RPT_1);
    LuaEngine::setAttrInt(L, "RPT_2",                       Icesat2Parms::RPT_2);
    LuaEngine::setAttrInt(L, "RPT_3",                       Icesat2Parms::RPT_3);
    LuaEngine::setAttrInt(L, "NUM_TRACKS",                  Icesat2Parms::NUM_TRACKS);
    LuaEngine::setAttrInt(L, "ATL08_NOISE",                 Icesat2Parms::ATL08_NOISE);
    LuaEngine::setAttrInt(L, "ATL08_GROUND",                Icesat2Parms::ATL08_GROUND);
    LuaEngine::setAttrInt(L, "ATL08_CANOPY",                Icesat2Parms::ATL08_CANOPY);
    LuaEngine::setAttrInt(L, "ATL08_TOP_OF_CANOPY",         Icesat2Parms::ATL08_TOP_OF_CANOPY);
    LuaEngine::setAttrInt(L, "ATL08_UNCLASSIFIED",          Icesat2Parms::ATL08_UNCLASSIFIED);

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
