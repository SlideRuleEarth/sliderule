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

#include "GeoUserRaster.h"
#include "MathLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* GeoUserRaster::RASTERDATA_KEY    = "data";
const char* GeoUserRaster::RASTERLENGTH_KEY  = "length";
const char* GeoUserRaster::GPSTIME_KEY       = "date";
const char* GeoUserRaster::ELEVATION_KEY     = "elevation";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - file(
 *  {
 *      file=<file>,
 *      filelength=<filelength>,
 *  })
 *----------------------------------------------------------------------------*/
int GeoUserRaster::luaCreate (lua_State* L)
{
    try
    {
        return createLuaObject(L, create(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoUserRaster: %s", e.what());
        return returnLuaStatus(L, false);
    }
}


/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoUserRaster* GeoUserRaster::create (lua_State* L, int index)
{
    /* Get raster */
    lua_getfield(L, index, RASTERDATA_KEY);
    const char* raster = getLuaString(L, -1);
    lua_pop(L, 1);

    /* Get raster length */
    lua_getfield(L, index, RASTERLENGTH_KEY);
    size_t rasterlength = (size_t)getLuaInteger(L, -1);
    lua_pop(L, 1);

    /* Get raster gps time */
    lua_getfield(L, index, GPSTIME_KEY);
    double gps = getLuaFloat(L, -1);
    lua_pop(L, 1);

    /* Get raster elevation flag */
    lua_getfield(L, index, ELEVATION_KEY);
    bool iselevation = getLuaBoolean(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, index, GeoParms::SELF);
    GeoParms* _parms = new GeoParms(L, lua_gettop(L), true);
    LuaObject::referenceLuaObject(_parms); // GeoUserRaster expects a LuaObject created from a Lua script
    lua_pop(L, 1);

    /* Convert raster from Base64 to Binary */
    std::string tiff = MathLib::b64decode(raster, rasterlength);

    const uint32_t maxSize = 64*1024*1024;
    if(rasterlength > maxSize)
        throw RunTimeException(CRITICAL, RTE_ERROR, "User raster too big, size is: %lu, max allowed: %u", rasterlength, maxSize);

    /* Create GeoUserRaster */
    return new GeoUserRaster(L, _parms, tiff.c_str(), tiff.size(), gps, iselevation);
}


/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoUserRaster::~GeoUserRaster(void)
{
    VSIUnlink(rasterFileName.c_str());
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoUserRaster::GeoUserRaster(lua_State *L, GeoParms* _parms, const char *file, long filelength, double gps, bool iselevation):
    GeoRaster(L, _parms, std::string("/vsimem/userraster/" + GdalRaster::getUUID() + ".tif"), gps, iselevation )
{
    rasterFileName = getFileName();

    if (file == NULL)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid file pointer (NULL)");

    if (filelength <= 0)
        throw RunTimeException(CRITICAL, RTE_ERROR, "Invalid filelength: %ld:", filelength);

    /* Load user raster to vsimem */
    VSILFILE* fp = VSIFileFromMemBuffer(rasterFileName.c_str(), (GByte*)file, (vsi_l_offset)filelength, FALSE);
    CHECKPTR(fp);
    VSIFCloseL(fp);

    openRaster();
}