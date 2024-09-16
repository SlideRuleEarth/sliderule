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

#include "OsApi.h"
#include "GeoLib.h"
#include "BathyMask.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyMask::GLOBAL_BATHYMETRY_MASK_FILE_PATH = "/data/ATL24_Mask_v5_Raster.tif";
const double BathyMask::GLOBAL_BATHYMETRY_MASK_MAX_LAT = 84.25;
const double BathyMask::GLOBAL_BATHYMETRY_MASK_MIN_LAT = -79.0;
const double BathyMask::GLOBAL_BATHYMETRY_MASK_MAX_LON = 180.0;
const double BathyMask::GLOBAL_BATHYMETRY_MASK_MIN_LON = -180.0;
const double BathyMask::GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE = 0.25;
const uint32_t BathyMask::GLOBAL_BATHYMETRY_MASK_OFF_VALUE = 0xFFFFFFFF;

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(...)
 *----------------------------------------------------------------------------*/
int BathyMask::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, new BathyMask(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating BathyMask: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * includes
 *----------------------------------------------------------------------------*/
bool BathyMask::includes (double lon, double lat)
{
    const double degrees_of_latitude = lat - GLOBAL_BATHYMETRY_MASK_MIN_LAT;
    const double latitude_pixels = degrees_of_latitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
    const uint32_t y = static_cast<uint32_t>(latitude_pixels);

    const double degrees_of_longitude =  lon - GLOBAL_BATHYMETRY_MASK_MIN_LON;
    const double longitude_pixels = degrees_of_longitude / GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
    const uint32_t x = static_cast<uint32_t>(longitude_pixels);

    const GeoLib::TIFFImage::val_t pixel = getPixel(x, y);
    return pixel.u32 == GLOBAL_BATHYMETRY_MASK_OFF_VALUE;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyMask::BathyMask (lua_State* L):
    GeoLib::TIFFImage(L, GLOBAL_BATHYMETRY_MASK_FILE_PATH)
{
}

