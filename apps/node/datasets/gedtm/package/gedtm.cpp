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
#include "gedtm.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_GEDTM_LIBNAME             "gedtm"
#define LUA_GEDTM30_METER_RASTER_NAME "gedtm-30meter"
#define LUA_GEDTM_STD_RASTER_NAME     "gedtm-std"       /* Standard deviation   */
#define LUA_GEDTM_DFM_RASTER_NAME     "gedtm-dfm"       /* Difference from mean */

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * gedtm_open
 *----------------------------------------------------------------------------*/
int gedtm_open (lua_State *L)
{
    static const struct luaL_Reg gedtm_functions[] = {
        {NULL, NULL}
    };

    /* Set Library */
    luaL_newlib(L, gedtm_functions);

    return 1;
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

extern "C" {
void initgedtm(void)
{
    /* Register Rasters */
    RasterObject::registerRaster(LUA_GEDTM30_METER_RASTER_NAME, GEDTM30meterRaster::create);
    RasterObject::registerRaster(LUA_GEDTM_STD_RASTER_NAME, GEDTMstdRaster::create);
    RasterObject::registerRaster(LUA_GEDTM_DFM_RASTER_NAME, GEDTMdfmRaster::create);

    /* Extend Lua */
    LuaEngine::extend(LUA_GEDTM_LIBNAME, gedtm_open, LIBID);

    /* Display Status */
    print2term("%s package initialized (%s)\n", LUA_GEDTM_LIBNAME, LIBID);
}

void deinitgedtm (void)
{
}
}
