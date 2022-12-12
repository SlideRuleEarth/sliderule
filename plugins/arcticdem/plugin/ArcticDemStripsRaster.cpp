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
 * INCLUDES
 ******************************************************************************/

#include "ArcticDemStripsRaster.h"

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate
 *----------------------------------------------------------------------------*/
int ArcticDemStripsRaster::luaCreate( lua_State* L )
{
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch( const RunTimeException& e )
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}



/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArcticDemStripsRaster::ArcticDemStripsRaster(lua_State *L, const char *dem_sampling, const int sampling_radius):
    VrtRaster(L, dem_sampling, sampling_radius)
{
    /*
     * For strips, there can be many rasters with the same point.
     * Some may be already cached but others not. First get a list of all rasters with point
     * and than see if some are already in cache.
     */
    checkCacheFirst    = false;

    /* Do not extrapolate for strips */
    extrapolateEnabled = false;
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
ArcticDemStripsRaster* ArcticDemStripsRaster::create( lua_State* L, int index )
{
    const int radius = getLuaInteger(L, -1);
    lua_pop(L, 1);
    const char* dem_sampling = getLuaString(L, -1);
    lua_pop(L, 1);
    return new ArcticDemStripsRaster(L, dem_sampling, radius);
}


/*----------------------------------------------------------------------------
 * getVrtFileName
 *----------------------------------------------------------------------------*/
void ArcticDemStripsRaster::getVrtFileName( double lon, double lat, std::string& vrtFile )
{
    int ilat = floor(lat);
    int ilon = floor(lon);

    vrtFile = "/data/ArcticDem/strips/n" +
              std::to_string(ilat) +
              ((ilon < 0) ? "w" : "e") +
              std::to_string(abs(ilon)) +
              ".vrt";
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/