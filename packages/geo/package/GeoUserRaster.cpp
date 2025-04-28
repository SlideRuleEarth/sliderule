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
const char* GeoUserRaster::SAMPLES_KEY       = "samples";

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
    const int index = 1;
    RequestFields* rqst_parms = NULL;
    try
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
        const double gps = getLuaFloat(L, -1);
        lua_pop(L, 1);

        /* Get raster elevation flag */
        lua_getfield(L, index, ELEVATION_KEY);
        const bool iselevation = getLuaBoolean(L, -1);
        lua_pop(L, 1);

        /* Get geo fields */
        lua_getfield(L, index, SAMPLES_KEY);
        rqst_parms = new RequestFields(L, 0, NULL, NULL, {});
        GeoFields* geo_fields = new GeoFields();
        if(!rqst_parms->samplers.add(GeoFields::DEFAULT_KEY, geo_fields))
        {
            delete geo_fields;
            throw RunTimeException(CRITICAL, RTE_FAILURE, "Failed to add default geo fields");
        }
        geo_fields->fromLua(L, lua_gettop(L));
        LuaObject::referenceLuaObject(rqst_parms); // GeoUserRaster expects a LuaObject created from a Lua script
        lua_pop(L, 1);

        /* Convert raster from Base64 to Binary */
        const std::string tiff = MathLib::b64decode(raster, rasterlength);
        rasterlength = tiff.length();

        /* Check maximum size */
        const uint32_t maxSize = 64*1024*1024;
        if(rasterlength > maxSize)
            throw RunTimeException(CRITICAL, RTE_FAILURE, "User raster too big, size is: %lu, max allowed: %u", rasterlength, maxSize);

        /* If raster has elevation assume it is in the first band */
        const int elevationBandNum = iselevation ? 1 : GdalRaster::NO_BAND;
        const int flagsBandNum = GdalRaster::NO_BAND;

        /* Create GeoUserRaster */
        return createLuaObject(L, new GeoUserRaster(L, rqst_parms, GeoFields::DEFAULT_KEY, tiff.c_str(), tiff.size(), gps, elevationBandNum, flagsBandNum));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoUserRaster: %s", e.what());
        delete rqst_parms;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoUserRaster::~GeoUserRaster(void)
{
    VSIUnlink(rasterFileName.c_str());
    free(data);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoUserRaster::GeoUserRaster(lua_State *L, RequestFields* rqst_parms, const char* key,
                             const char *file, long filelength, double gps,
                             int elevationBandNum, int flagsBandNum) :
    GeoRaster(L, rqst_parms, key, std::string("/vsimem/userraster/" + GdalRaster::getUUID() + ".tif"), gps, elevationBandNum, flagsBandNum),
    data(NULL)
{
    if(file == NULL)
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid file pointer (NULL)");

    if(filelength <= 0)
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Invalid filelength: %ld:", filelength);

    try
    {
        rasterFileName = getFileName();

        /* Make a copy of the raster data and pass the ownership to the VSIFile */
        data = reinterpret_cast<GByte*>(malloc(filelength));
        memcpy(data, file, filelength);

        /* Load user raster to vsimem */
        const bool takeOwnership = false;
        VSILFILE* fp = VSIFileFromMemBuffer(rasterFileName.c_str(), data, static_cast<vsi_l_offset>(filelength), takeOwnership);
        CHECKPTR(fp);
        VSIFCloseL(fp);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating GeoUserRaster: %s", e.what());
    }
}
