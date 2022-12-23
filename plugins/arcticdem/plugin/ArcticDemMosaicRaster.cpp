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

#include "ArcticDemMosaicRaster.h"

/******************************************************************************
 * PRIVATE IMPLEMENTATION
 ******************************************************************************/

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArcticDemMosaicRaster::ArcticDemMosaicRaster(lua_State *L, const char *dem_sampling, const int sampling_radius):
    VrtRaster(L, dem_sampling, sampling_radius)
{
    /* There is only one mosaic VRT file. Open it. */
    if (!openVrtDset())
        throw RunTimeException(CRITICAL, RTE_ERROR, "Constructor %s failed", __FUNCTION__);

    /*
     * For mosaic, there is only one raster with point in it.
     * Find it in cache first, before looking in vrt file for new tif.
     */
    checkCacheFirst = true;
}

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
VrtRaster* ArcticDemMosaicRaster::create(lua_State* L, const char* dem_sampling, const int sampling_radius)
{
    return new ArcticDemMosaicRaster(L, dem_sampling, sampling_radius);
}


/*----------------------------------------------------------------------------
 * getVrtFileName
 *----------------------------------------------------------------------------*/
void ArcticDemMosaicRaster::getVrtFileName(std::string& vrtFile, double lon, double lat)
{
    vrtFile = "/data/ArcticDem/mosaic.vrt";
}


/*----------------------------------------------------------------------------
 * getDateTokens
 *----------------------------------------------------------------------------*/
void ArcticDemMosaicRaster::getDateTokens(std::string &key, std::string &fieldName, std::string& fileType)
{
    key       = "_reg_dem.tif";
    fieldName = "end_datetime";
    fileType  = ".json";
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/



