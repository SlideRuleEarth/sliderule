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
#include "gedi.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEDI_LIBNAME                            "gedi"
#define LUA_GEDI_L03_ELEVATION_RASTER_NAME          "gedil3-elevation"
#define LUA_GEDI_L03_CANOPY_RASTER_NAME             "gedil3-canopy"
#define LUA_GEDI_L03_ELEVATION_STDDEV_RASTER_NAME   "gedil3-elevation-stddev"
#define LUA_GEDI_L03_CANOPY_STDDEV_RASTER_NAME      "gedil3-canopy-stddev"
#define LUA_GEDI_L03_COUNTS_RASTER_NAME             "gedil3-counts"
#define LUA_GEDI_L04B_RASTER_NAME                   "gedil4b"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * gedi_version
 *----------------------------------------------------------------------------*/
int gedi_version (lua_State* L)
{
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * gedi_open
 *----------------------------------------------------------------------------*/
int gedi_open (lua_State *L)
{
    static const struct luaL_Reg gedi_functions[] = {
        {"parms",               GediParms::luaCreate},
        {"gedi01b",             Gedi01bReader::luaCreate},
        {"gedi02a",             Gedi02aReader::luaCreate},
        {"gedi04a",             Gedi04aReader::luaCreate},
        {"version",             gedi_version},
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, gedi_functions);

    /* Set Globals */
    LuaEngine::setAttrInt(L, "NUM_BEAMS",       GediParms::NUM_BEAMS);
    LuaEngine::setAttrInt(L, "ALL_BEAMS",       GediParms::ALL_BEAMS);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initgedi (void)
{
    /* Initialize Modules */
    Gedi01bReader::init();
    Gedi02aReader::init();
    Gedi03Raster::init();
    Gedi04aReader::init();
    Gedi04bRaster::init();

    /* Register Rasters */
    RasterObject::registerRaster(LUA_GEDI_L03_ELEVATION_RASTER_NAME,        Gedi03Raster::create);
    RasterObject::registerRaster(LUA_GEDI_L03_CANOPY_RASTER_NAME,           Gedi03Raster::create);
    RasterObject::registerRaster(LUA_GEDI_L03_ELEVATION_STDDEV_RASTER_NAME, Gedi03Raster::create);
    RasterObject::registerRaster(LUA_GEDI_L03_CANOPY_STDDEV_RASTER_NAME,    Gedi03Raster::create);
    RasterObject::registerRaster(LUA_GEDI_L03_COUNTS_RASTER_NAME,           Gedi03Raster::create);
    RasterObject::registerRaster(LUA_GEDI_L04B_RASTER_NAME,                 Gedi04bRaster::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_GEDI_LIBNAME, gedi_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEDI_LIBNAME, BINID);

    /* Display Status */
    print2term("%s plugin initialized (%s)\n", LUA_GEDI_LIBNAME, BINID);
}

void deinitgedi (void)
{
}
}
