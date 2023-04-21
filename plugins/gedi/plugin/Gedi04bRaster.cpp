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
#include "geo.h"
#include "Gedi04bRaster.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void Gedi04bRaster::init(void)
{
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
GeoRaster* Gedi04bRaster::create(lua_State* L, GeoParms* _parms)
{ 
    return new Gedi04bRaster(L, _parms); 
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi04bRaster::~Gedi04bRaster(void)
{
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi04bRaster::Gedi04bRaster(lua_State *L, GeoParms* _parms):
    VrtRaster(L, _parms, std::string("/vsimem/" + std::string(getUUID(uuid_str)) + ".vrt").c_str())
{
    /* Initialize Date */
    TimeLib::gmt_time_t gmtDate = {
        .year = 2021,
        .doy = 216,
        .hour = 0,
        .minute = 0,
        .second = 0,
        .millisecond = 0
    };

    /* Initialize Time */
    gpsTime = TimeLib::gmt2gpstime(gmtDate);

    /* Create VRT File with Raster */
    try
    {
        List<std::string> rasterList;
        std::string rasterFile = std::string(_parms->asset->getPath()) + "/" + std::string(_parms->asset->getIndex());
        rasterList.add(rasterFile);
        buildVRT(vrtFile, rasterList);
        openGeoIndex();
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Gedi04bRaster: %s", e.what());
    }
}

/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
bool Gedi04bRaster::getRasterDate(raster_info_t& rinfo)
{
    rinfo.gmtDate = gmtDate;
    rinfo.gpsTime = gpsTime;
    return true;
}
