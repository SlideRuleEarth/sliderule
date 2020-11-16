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
 * icesat2_open
 *----------------------------------------------------------------------------*/
int icesat2_open (lua_State *L)
{
    static const struct luaL_Reg icesat2_functions[] = {
        {"atl03",           Atl03Reader::luaCreate},
        {"atl03indexer",    Atl03Indexer::luaCreate},
        {"atl06",           Atl06Dispatch::luaCreate},
        {"ut_atl06",        UT_Atl06Dispatch::luaCreate},
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, icesat2_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "CNF_POSSIBLE_TEP",    CNF_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "CNF_NOT_CONSIDERED",  CNF_NOT_CONSIDERED);
    LuaEngine::setAttrInt(L, "CNF_BACKGROUND",      CNF_BACKGROUND);
    LuaEngine::setAttrInt(L, "CNF_WITHIN_10M",      CNF_WITHIN_10M);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_LOW",     CNF_SURFACE_LOW);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_MEDIUM",  CNF_SURFACE_MEDIUM);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_HIGH",    CNF_SURFACE_HIGH);
    LuaEngine::setAttrInt(L, "SRT_LAND",            SRT_LAND);
    LuaEngine::setAttrInt(L, "SRT_OCEAN",           SRT_OCEAN);
    LuaEngine::setAttrInt(L, "SRT_SEA_ICE",         SRT_SEA_ICE);
    LuaEngine::setAttrInt(L, "SRT_LAND_ICE",        SRT_LAND_ICE);
    LuaEngine::setAttrInt(L, "SRT_INLAND_WATER",    SRT_INLAND_WATER);
    LuaEngine::setAttrInt(L, "STAGE_SUB",           STAGE_SUB);
    LuaEngine::setAttrInt(L, "STAGE_RAW",           STAGE_RAW);
    LuaEngine::setAttrInt(L, "STAGE_LSF",           STAGE_LSF);
    LuaEngine::setAttrInt(L, "ALL_STAGES",          NUM_STAGES);
    LuaEngine::setAttrInt(L, "ALL_TRACKS",          ALL_TRACKS);
    LuaEngine::setAttrInt(L, "RPT_1",               RPT_1);
    LuaEngine::setAttrInt(L, "RPT_2",               RPT_2);
    LuaEngine::setAttrInt(L, "RPT_3",               RPT_3);
    LuaEngine::setAttrInt(L, "NUM_TRACKS",          NUM_TRACKS);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initicesat2 (void)
{
    /* Initialize Modules */
    Atl03Reader::init();
    Atl03Indexer::init();
    Atl06Dispatch::init();

    /* Extend Lua */
    LuaEngine::extend(LUA_ICESAT2_LIBNAME, icesat2_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_ICESAT2_LIBNAME, BINID);

    /* Display Status */
    printf("%s plugin initialized (%s)\n", LUA_ICESAT2_LIBNAME, BINID);
}
}
