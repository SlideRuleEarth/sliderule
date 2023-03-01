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
#include "pgc.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PGC_LIBNAME                    "pgc"
#define LUA_ARCTIC_DEM_MOSAIC_RASTER_NAME  "arcticdem-mosaic"
#define LUA_ARCTIC_DEM_STRIPS_RASTER_NAME  "arcticdem-strips"
#define LUA_REMA_DEM_MOSAIC_RASTER_NAME    "rema-mosaic"
#define LUA_REMA_DEM_STRIPS_RASTER_NAME    "rema-strips"


/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * pgc_version
 *----------------------------------------------------------------------------*/
int pgc_version (lua_State* L)
{
    /* Display Version Information on Terminal */
    print2term("PGC Plugin Version: %s\n", BINID);
    print2term("Build Information: %s\n", BUILDINFO);

    /* Return Version Information to Lua */
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * pgc_open
 *----------------------------------------------------------------------------*/
int pgc_open (lua_State *L)
{
    static const struct luaL_Reg pgc_functions[] = {
        {"version",         pgc_version},
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, pgc_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initpgc(void)
{
    /* Initialize Modules */
    ArcticDemMosaicRaster::init();
    ArcticDemStripsRaster::init();
    RemaDemMosaicRaster::init();
    // RemaDemStripsRaster::init();

    /* Register Rasters */
    GeoRaster::registerRaster(LUA_ARCTIC_DEM_MOSAIC_RASTER_NAME, ArcticDemMosaicRaster::create);
    GeoRaster::registerRaster(LUA_ARCTIC_DEM_STRIPS_RASTER_NAME, ArcticDemStripsRaster::create);
    GeoRaster::registerRaster(LUA_REMA_DEM_MOSAIC_RASTER_NAME, RemaDemMosaicRaster::create);
    // GeoRaster::registerRaster(LUA_REMA_DEM_STRIPS_RASTER_NAME, RemaDemStripsRaster::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_PGC_LIBNAME, pgc_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_PGC_LIBNAME, BINID);

    /* Display Status */
    print2term("%s plugin initialized (%s)\n", LUA_PGC_LIBNAME, BINID);
}

void deinitpgc (void)
{
    /* Uninitialize Modules */
    ArcticDemMosaicRaster::deinit();
    ArcticDemStripsRaster::deinit();
    RemaDemMosaicRaster::deinit();
    // RemaDemStripsRaster::deinit();
}
}
