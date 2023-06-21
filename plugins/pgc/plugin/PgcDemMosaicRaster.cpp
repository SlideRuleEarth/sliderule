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

#include "PgcDemMosaicRaster.h"
#include "TimeLib.h"

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
PgcDemMosaicRaster::PgcDemMosaicRaster(lua_State *L, GeoParms* _parms):
    VrtRaster(L, _parms)
{
    /*
     * Pgc Mosaics uses one large VRT index file but we cannot open it here.
     * Derived classes (Arctic/Rema) override overrideTargetCRS() function.
     * If we open VRT index file here the wrong overrideTargetCRS() will be called
     * resulting in incorrect CRS transform.
     */

     //openGeoIndex();
}


/*----------------------------------------------------------------------------
 * getRasterDate
 *----------------------------------------------------------------------------*/
bool PgcDemMosaicRaster::mosaicGetRasterDate(raster_info_t& rinfo, int year, int month, int day, int hour, int minute, int second)
{
    rinfo.gmtDate.year        = year;
    rinfo.gmtDate.doy         = TimeLib::dayofyear(year, month, day);
    rinfo.gmtDate.hour        = hour;
    rinfo.gmtDate.minute      = minute;
    rinfo.gmtDate.second      = second;
    rinfo.gmtDate.millisecond = 0;
    rinfo.gpsTime             = TimeLib::gmt2gpstime(rinfo.gmtDate);
    return true;
}

