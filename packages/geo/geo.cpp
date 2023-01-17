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
#include "geo.h"
#include "GeoJsonRaster.h"
#include <gdal.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEO_LIBNAME  "geo"

/******************************************************************************
 * GEO FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * geo_open
 *----------------------------------------------------------------------------*/
int geo_open (lua_State* L)
{
    static const struct luaL_Reg geo_functions[] = {
        {"geojson",     GeoJsonRaster::luaCreate},
        {"raster",      GeoRaster::luaCreate},
        {NULL,          NULL}
    };

    /* Set Package Library */
    luaL_newlib(L, geo_functions);

    /* Set Globals */
    LuaEngine::setAttrStr   (L, GeoRaster::NEARESTNEIGHBOUR_ALGO,   GeoRaster::NEARESTNEIGHBOUR_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::BILINEAR_ALGO,           GeoRaster::BILINEAR_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::CUBIC_ALGO,              GeoRaster::CUBIC_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::CUBICSPLINE_ALGO,        GeoRaster::CUBICSPLINE_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::LANCZOS_ALGO,            GeoRaster::LANCZOS_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::AVERAGE_ALGO,            GeoRaster::AVERAGE_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::MODE_ALGO,               GeoRaster::MODE_ALGO);
    LuaEngine::setAttrStr   (L, GeoRaster::GAUSS_ALGO,              GeoRaster::GAUSS_ALGO);

    return 1;
}


/*----------------------------------------------------------------------------
 * Error handler called by GDAL lib on errors
 *----------------------------------------------------------------------------*/
void GdalErrHandler(CPLErr eErrClass, int err_no, const char *msg)
{
    (void)eErrClass;  /* Silence compiler warning */
    mlog(CRITICAL, "GDAL ERROR %d: %s", err_no, msg);
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/
extern "C" {
void initgeo (void)
{
    /* Register all gdal drivers */
    GDALAllRegister();

    /* Initialize Modules */
    GeoRaster::init();

    /* Register GDAL custom error handler */
    void (*fptrGdalErrorHandler)(CPLErr, int, const char *) = GdalErrHandler;
    CPLSetErrorHandler(fptrGdalErrorHandler);

    /* Extend Lua */
    LuaEngine::extend(LUA_GEO_LIBNAME, geo_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEO_LIBNAME, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_GEO_LIBNAME, LIBID);
}

void deinitgeo (void)
{
    GeoRaster::deinit();
    GDALDestroy();
}
}
