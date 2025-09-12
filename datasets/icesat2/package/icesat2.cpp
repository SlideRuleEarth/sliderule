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

#include "LuaEngine.h"
#include "RasterObject.h"
#include "Asset.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"
#include "Atl03Reader.h"
#include "Atl03Viewer.h"
#include "Atl03Indexer.h"
#include "Atl06Reader.h"
#include "Atl06Dispatch.h"
#include "Atl08Dispatch.h"
#include "Atl13DataFrame.h"
#include "Atl13IODriver.h"
#include "Atl13Reader.h"
#include "Atl24DataFrame.h"
#include "Atl24Granule.h"
#include "Atl24IODriver.h"
#include "CumulusIODriver.h"
#include "MeritRaster.h"
#include "PhoReal.h"
#include "SurfaceFitter.h"
#ifdef __unittesting__
#include "UT_Atl06Dispatch.h"
#endif

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
        {"parms",               Icesat2Fields::luaCreate},
        {"atl03s",              Atl03Reader::luaCreate},
        {"atl03v",              Atl03Viewer::luaCreate},
        {"atl03indexer",        Atl03Indexer::luaCreate},
        {"atl06",               Atl06Dispatch::luaCreate},
        {"atl06s",              Atl06Reader::luaCreate},
        {"atl08",               Atl08Dispatch::luaCreate},
        {"atl13s",              Atl13Reader::luaCreate},
        // dataframes
        {"atl03x",              Atl03DataFrame::luaCreate},
        {"fit",                 SurfaceFitter::luaCreate},
        {"phoreal",             PhoReal::luaCreate},
        {"atl13x",              Atl13DataFrame::luaCreate},
        {"atl24x",              Atl24DataFrame::luaCreate},
        {"atl24granule",        Atl24Granule::luaCreate},
#ifdef __unittesting__
        {"ut_atl06",            UT_Atl06Dispatch::luaCreate},
#endif
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, icesat2_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "CNF_POSSIBLE_TEP",            Icesat2Fields::CNF_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "CNF_NOT_CONSIDERED",          Icesat2Fields::CNF_NOT_CONSIDERED);
    LuaEngine::setAttrInt(L, "CNF_BACKGROUND",              Icesat2Fields::CNF_BACKGROUND);
    LuaEngine::setAttrInt(L, "CNF_WITHIN_10M",              Icesat2Fields::CNF_WITHIN_10M);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_LOW",             Icesat2Fields::CNF_SURFACE_LOW);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_MEDIUM",          Icesat2Fields::CNF_SURFACE_MEDIUM);
    LuaEngine::setAttrInt(L, "CNF_SURFACE_HIGH",            Icesat2Fields::CNF_SURFACE_HIGH);
    LuaEngine::setAttrInt(L, "QUALITY_NOMINAL",             Icesat2Fields::QUALITY_NOMINAL);
    LuaEngine::setAttrInt(L, "QUALITY_AFTERPULSE",          Icesat2Fields::QUALITY_POSSIBLE_AFTERPULSE);
    LuaEngine::setAttrInt(L, "QUALITY_IMPULSE_RESPONSE",    Icesat2Fields::QUALITY_POSSIBLE_IMPULSE_RESPONSE);
    LuaEngine::setAttrInt(L, "QUALITY_POSSIBLE_TEP",        Icesat2Fields::QUALITY_POSSIBLE_TEP);
    LuaEngine::setAttrInt(L, "SRT_LAND",                    Icesat2Fields::SRT_LAND);
    LuaEngine::setAttrInt(L, "SRT_OCEAN",                   Icesat2Fields::SRT_OCEAN);
    LuaEngine::setAttrInt(L, "SRT_SEA_ICE",                 Icesat2Fields::SRT_SEA_ICE);
    LuaEngine::setAttrInt(L, "SRT_LAND_ICE",                Icesat2Fields::SRT_LAND_ICE);
    LuaEngine::setAttrInt(L, "SRT_INLAND_WATER",            Icesat2Fields::SRT_INLAND_WATER);
    LuaEngine::setAttrInt(L, "ALL_TRACKS",                  Icesat2Fields::ALL_TRACKS);
    LuaEngine::setAttrInt(L, "RPT_1",                       Icesat2Fields::RPT_1);
    LuaEngine::setAttrInt(L, "RPT_2",                       Icesat2Fields::RPT_2);
    LuaEngine::setAttrInt(L, "RPT_3",                       Icesat2Fields::RPT_3);
    LuaEngine::setAttrInt(L, "NUM_TRACKS",                  Icesat2Fields::NUM_TRACKS);
    LuaEngine::setAttrInt(L, "NUM_SPOTS",                   Icesat2Fields::NUM_SPOTS);
    LuaEngine::setAttrInt(L, "ATL08_NOISE",                 Icesat2Fields::ATL08_NOISE);
    LuaEngine::setAttrInt(L, "ATL08_GROUND",                Icesat2Fields::ATL08_GROUND);
    LuaEngine::setAttrInt(L, "ATL08_CANOPY",                Icesat2Fields::ATL08_CANOPY);
    LuaEngine::setAttrInt(L, "ATL08_TOP_OF_CANOPY",         Icesat2Fields::ATL08_TOP_OF_CANOPY);
    LuaEngine::setAttrInt(L, "ATL08_UNCLASSIFIED",          Icesat2Fields::ATL08_UNCLASSIFIED);
    LuaEngine::setAttrInt(L, "ATL06",                       Icesat2Fields::STAGE_ATL06);
    LuaEngine::setAttrInt(L, "ATL08",                       Icesat2Fields::STAGE_ATL08);
    LuaEngine::setAttrInt(L, "YAPC",                        Icesat2Fields::STAGE_YAPC);
    LuaEngine::setAttrInt(L, "PHOREAL",                     Icesat2Fields::STAGE_PHOREAL);
    LuaEngine::setAttrInt(L, "ATL24",                       Icesat2Fields::STAGE_ATL24);

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
    Atl03Viewer::init();
    Atl03Indexer::init();
    Atl06Dispatch::init();
    Atl06Reader::init();
    Atl08Dispatch::init();
    Atl13Reader::init();

    /* Load CRS Files */
    Icesat2Fields::loadCRSFiles();

    /* Register IO Drivers */
    Asset::registerDriver(CumulusIODriver::FORMAT, CumulusIODriver::create);
    Asset::registerDriver(Atl13IODriver::FORMAT, Atl13IODriver::create);
    Asset::registerDriver(Atl24IODriver::FORMAT, Atl24IODriver::create);

    /* Register Rasters */
    RasterObject::registerRaster(MeritRaster::ASSET_NAME, MeritRaster::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_ICESAT2_LIBNAME, icesat2_open, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_ICESAT2_LIBNAME, LIBID);
}

void deiniticesat2 (void)
{
}
}
