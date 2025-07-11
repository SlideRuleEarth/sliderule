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

#include "OsApi.h"
#include "FootprintReader.h"
#include "Gedi01bReader.h"
#include "Gedi02aReader.h"
#include "Gedi04aReader.h"
#include "GediRaster.h"
#include "GediFields.h"
#include "GediIODriver.h"

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
 * gedi_open
 *----------------------------------------------------------------------------*/
int gedi_open (lua_State *L)
{
    static const struct luaL_Reg gedi_functions[] = {
        {"parms",               GediFields::luaCreate},
        {"gedi01b",             Gedi01bReader::luaCreate},
        {"gedi02a",             Gedi02aReader::luaCreate},
        {"gedi04a",             Gedi04aReader::luaCreate},
        {NULL,                  NULL}
    };

    /* Set Library */
    luaL_newlib(L, gedi_functions);

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
    Gedi04aReader::init();

    /* Register GEDI IO Driver */
    Asset::registerDriver(GediIODriver::FORMAT, GediIODriver::create);

    /* Register Rasters */
    RasterObject::registerRaster(LUA_GEDI_L03_ELEVATION_RASTER_NAME,        GediRaster::createL3ElevationRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_CANOPY_RASTER_NAME,           GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_ELEVATION_STDDEV_RASTER_NAME, GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_CANOPY_STDDEV_RASTER_NAME,    GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L03_COUNTS_RASTER_NAME,           GediRaster::createL3DataRaster);
    RasterObject::registerRaster(LUA_GEDI_L04B_RASTER_NAME,                 GediRaster::createL4DataRaster);

    /* Extend Lua */
    LuaEngine::extend(LUA_GEDI_LIBNAME, gedi_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEDI_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_GEDI_LIBNAME, LIBID);
}

void deinitgedi (void)
{
}
}
