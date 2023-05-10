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

#include "core.h"
#include "h5.h"
#include "geo.h"
#include "MeritRaster.h"

#include <math.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MeritRaster::ASSET_NAME = "merit";
const char* MeritRaster::RESOURCE_NAME = "merit_3as_20200617_001_01.h5";

const double MeritRaster::X_SCALE = (1.0 / 1200.0);
const double MeritRaster::Y_SCALE = (-1.0 / 1200.0);

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void MeritRaster::init(void)
{
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
RasterObject* MeritRaster::create(lua_State* L, GeoParms* _parms)
{ 
    return new MeritRaster(L, _parms); 
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
MeritRaster::~MeritRaster(void)
{
    if(asset) asset->releaseLuaObject();
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MeritRaster::MeritRaster(lua_State *L, GeoParms* _parms):
    RasterObject(L, _parms),
    asset(NULL),
    cacheLat(0),
    cacheLon(0),
    cacheSize(0),
    cache(NULL)
{
    /* Initialize Time */
    TimeLib::gmt_time_t gmt_date = {
        .year = 2020,
        .doy = 169,
        .hour = 0,
        .minute = 0,
        .second = 0,
        .millisecond = 0
    };
    gpsTime = TimeLib::gmt2gpstime(gmt_date);

    /*  Inintialize Asset */
    asset = (Asset*)LuaObject::getLuaObjectByName(ASSET_NAME, Asset::OBJECT_TYPE);
    if(!asset) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to find asset %s", ASSET_NAME);
}

/*----------------------------------------------------------------------------
 * getSamples
 *----------------------------------------------------------------------------*/
void MeritRaster::getSamples (double lon, double lat, int64_t gps, List<sample_t>& slist, void* param)
{
    (void)param;
    (void)gps;

    /* Determine Upper Left Coordinates */
    int left_lon = ((int)floor(lon / 5.0)) * 5;
    int upper_lat = ((int)ceil(lat / 5.0)) * 5;
    
    /* Calculate Pixel Location */
    int x_offset = (int)(((double)lon - left_lon) / X_SCALE);
    int y_offset = (int)(((double)lat - upper_lat) / Y_SCALE);

    /* Check Pixel Location */
    if( x_offset < 0 || x_offset >= X_MAX ||
        y_offset < 0 || y_offset >= Y_MAX )
    {
        mlog(ERROR, "Invalid pixel location for MERIT DEM at %lf, %lf: %d, %d\n", lon, lat, x_offset, y_offset);
        return;
    }

    /* Handle Negative Longitudes */
    char char4lon = 'e';
    if(left_lon < 0)
    {
        char4lon = 'w';
        left_lon *= -1;
    }

    /* Handle Negative Latitudes */
    char char4lat = 'n';
    if(upper_lat < 0)
    {
        char4lon = 's';
        upper_lat *= -1;
    }

    /* Build Dataset Name */
    SafeString dataset("%c%02d%c%03d_MERITdem_wgs84", char4lat, upper_lat, char4lon, left_lon);

    /* Read Dataset */
    H5Coro::context_t context;
    H5Array<float> tile(asset, RESOURCE_NAME, dataset.str(), &context);

    /* Build Sample */
    sample_t sample = {
        .fileId = 0,
        .flags = 0,
        .time = gpsTime,
        .value = tile[(y_offset *  X_MAX) + x_offset]
    };
    memset(&sample.stats, 0, sizeof(sample.stats));

    /* Return Sample */
    slist.add(sample);
}
