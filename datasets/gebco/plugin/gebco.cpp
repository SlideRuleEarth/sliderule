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
#include "gebco.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEBCO_LIBNAME     "gebco"
#define LUA_GEBCO_RASTER_NAME "gebco-bathy"

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * gebco_version
 *----------------------------------------------------------------------------*/
int gebco_version (lua_State* L)
{
    lua_pushstring(L, BINID);
    lua_pushstring(L, BUILDINFO);
    return 2;
}

/*----------------------------------------------------------------------------
 * gebco_open
 *----------------------------------------------------------------------------*/
int gebco_open (lua_State *L)
{
    static const struct luaL_Reg gebco_functions[] = {
        {"version",         gebco_version},
        {NULL,              NULL}
    };

    /* Set Library */
    luaL_newlib(L, gebco_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initgebco(void)
{
    /* Initialize Modules */
    GebcoBathyRaster::init();

    /* Register Rasters */
    RasterObject::registerRaster(LUA_GEBCO_RASTER_NAME, GebcoBathyRaster::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_GEBCO_LIBNAME, gebco_open);

    /* Indicate Presence of Package */
    LuaEngine::indicate(LUA_GEBCO_LIBNAME, BINID);

    /* Display Status */
    print2term("%s plugin initialized (%s)\n", LUA_GEBCO_LIBNAME, BINID);
}

void deinitgebco(void)
{
    /* Uninitialize Modules */
    GebcoBathyRaster::deinit();
}
}
