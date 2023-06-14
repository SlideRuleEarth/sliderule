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
#include "usgs3dep.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_USGS3DEP_LIBNAME                "usgs3dep"
#define LUA_USGS3DEP_1METER_DEM_RASTER_NAME "usgs3dep-1meter-dem"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * usgs3dep_version
 *----------------------------------------------------------------------------*/
int usgs3dep_version (lua_State* L)
{
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * usgs3dep_open
 *----------------------------------------------------------------------------*/
int usgs3dep_open (lua_State *L)
{
    static const struct luaL_Reg usgs3dep_functions[] = {
        {"version",         usgs3dep_version},
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, usgs3dep_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initusgs3dep(void)
{
    /* Initialize Modules */
    Usgs3dep1meterDemRaster::init();

    /* Register Rasters */
    RasterObject::registerRaster(LUA_USGS3DEP_1METER_DEM_RASTER_NAME, Usgs3dep1meterDemRaster::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_USGS3DEP_LIBNAME, usgs3dep_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_USGS3DEP_LIBNAME, BINID);

    /* Display Status */
    print2term("%s plugin initialized (%s)\n", LUA_USGS3DEP_LIBNAME, BINID);
}

void deinitusgs3dep (void)
{
    /* Uninitialize Modules */
    Usgs3dep1meterDemRaster::deinit();
}
}
