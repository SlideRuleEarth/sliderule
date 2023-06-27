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

#include <bits/stdint-intn.h>
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
RasterObject* Gedi04bRaster::create(lua_State* L, GeoParms* _parms)
{
    return new Gedi04bRaster(L, _parms);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Gedi04bRaster::~Gedi04bRaster(void)
{
    delete raster;
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Gedi04bRaster::Gedi04bRaster(lua_State *L, GeoParms* _parms):
    GeoRaster(L, _parms)
{
    TimeLib::gmt_time_t gmtDate;

    /* Initialize Date */
    gmtDate = {
        .year = 2021,
        .doy = 216,
        .hour = 0,
        .minute = 0,
        .second = 0,
        .millisecond = 0
    };

    /* Initialize Time */
    int64_t gpsTime = TimeLib::gmt2gpstime(gmtDate);

    try
    {
        std::string rasterFile = std::string(_parms->asset->getPath()) + "/" + std::string(_parms->asset->getIndex());

        /* Open raster for sampling */
        raster = new GeoRaster::Raster(rasterFile.c_str(), gpsTime);
        open(raster);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Gedi04bRaster: %s", e.what());
    }
}
