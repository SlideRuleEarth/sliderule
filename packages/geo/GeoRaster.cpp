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

#include "GeoRaster.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void GeoRaster::init (void)
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void GeoRaster::deinit (void)
{
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GeoRaster::GeoRaster(lua_State *L, RequestFields* rqst_parms, const char* key, const std::string& _fileName,
                     double _gpsTime, int elevationBandNum, int flagsBandNum, GdalRaster::overrideCRS_t cb):
    RasterObject(L, rqst_parms, key),
    raster(parms, _fileName, _gpsTime, fileDict.add(_fileName, true), elevationBandNum, flagsBandNum, cb)
{
    /* Add Lua Functions */
    LuaEngine::setAttrFunc(L, "dim", luaDimensions);
    LuaEngine::setAttrFunc(L, "bbox", luaBoundingBox);
    LuaEngine::setAttrFunc(L, "cell", luaCellSize);

    /* Establish Credentials */
    GdalRaster::initAwsAccess(parms);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
GeoRaster::~GeoRaster(void) = default;


/*----------------------------------------------------------------------------
 * getSamples
 *----------------------------------------------------------------------------*/
uint32_t GeoRaster::getSamples(const MathLib::point_3d_t& point, int64_t gps, List<RasterSample*>& slist, void* param)
{
    static_cast<void>(gps);
    static_cast<void>(param);

    lockSampling();
    try
    {
        std::vector<int> bands;
        getInnerBands(&raster, bands);
        for(const int bandNum : bands)
        {
            /* Must create OGRPoint for each bandNum, samplePOI projects it to raster CRS */
            OGRPoint ogr_point(point.x, point.y, point.z);

            RasterSample* sample = raster.samplePOI(&ogr_point, bandNum);
            if(sample) slist.add(sample);
        }
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting samples: %s", e.what());
    }
    unlockSampling();

    return raster.getSSerror();
}

/*----------------------------------------------------------------------------
 * getPixels
 *----------------------------------------------------------------------------*/
uint8_t* GeoRaster::getPixels(uint32_t ulx, uint32_t uly, uint32_t xsize, uint32_t ysize, int bandNum, void* param)
{
    static_cast<void>(param);
    uint8_t* data = NULL;

    lockSampling();

    /* Enable multi-threaded decompression in Gtiff driver */
    CPLSetThreadLocalConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");

    data = raster.getPixels(ulx, uly, xsize, ysize, bandNum);

    /* Disable multi-threaded decompression in Gtiff driver */
    CPLSetThreadLocalConfigOption("GDAL_NUM_THREADS", "1");

    unlockSampling();

    return data;
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaDimensions - :dim() --> rows, cols
 *----------------------------------------------------------------------------*/
int GeoRaster::luaDimensions(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoRaster *lua_obj = dynamic_cast<GeoRaster*>(getLuaSelf(L, 1));

        /* Set Return Values */
        lua_pushinteger(L, lua_obj->raster.getRows());
        lua_pushinteger(L, lua_obj->raster.getCols());
        num_ret += 2;

        /* Set Return Status */
        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting dimensions: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaBoundingBox - :bbox() --> (lon_min, lat_min, lon_max, lat_max)
 *----------------------------------------------------------------------------*/
int GeoRaster::luaBoundingBox(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoRaster *lua_obj = dynamic_cast<GeoRaster*>(getLuaSelf(L, 1));

        /* Set Return Values */
        const GdalRaster::bbox_t bbox = lua_obj->raster.getBbox();
        lua_pushnumber(L, bbox.lon_min);
        lua_pushnumber(L, bbox.lat_min);
        lua_pushnumber(L, bbox.lon_max);
        lua_pushnumber(L, bbox.lat_max);
        num_ret += 4;

        /* Set Return Status */
        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting bounding box: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}

/*----------------------------------------------------------------------------
 * luaCellSize - :cell() --> cell size
 *----------------------------------------------------------------------------*/
int GeoRaster::luaCellSize(lua_State *L)
{
    bool status = false;
    int num_ret = 1;

    try
    {
        /* Get Self */
        GeoRaster *lua_obj = dynamic_cast<GeoRaster*>(getLuaSelf(L, 1));

        /* Set Return Values */
        lua_pushnumber(L, lua_obj->raster.getCellSize());
        num_ret += 1;

        /* Set Return Status */
        status = true;
    }
    catch (const RunTimeException &e)
    {
        mlog(e.level(), "Error getting cell size: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}
