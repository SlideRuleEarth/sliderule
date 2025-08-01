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

#ifndef __remadem_mosaic_raster__
#define __remadem_mosaic_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoRaster.h"
#include "PgcWkt.h"

/******************************************************************************
 * REMA DEM MOSAIC RASTER CLASS
 ******************************************************************************/

class RemaDemMosaicRaster: public GeoRaster
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static RasterObject* create(lua_State* L, RequestFields* rqst_parms, const char* key)
        { return new RemaDemMosaicRaster(L, rqst_parms, key); }


    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        RemaDemMosaicRaster(lua_State* L, RequestFields* rqst_parms, const char* key):
         GeoRaster(L, rqst_parms, key,
                  std::string(rqst_parms->geoFields(key)->asset.asset->getIndex()),
                  TimeLib::datetime2gps(2023, 02, 24, 18, 51, 44) / 1000,
                  1,                   /* elevationBandNum */
                  GdalRaster::NO_BAND, /* flagsBandNum     */
                  NULL,                /* overrideGeoTransform */
                  &overrideTargetCRS) {}

        static OGRErr overrideTargetCRS(OGRSpatialReference& target, const void* param=NULL)
        {
            static_cast<void>(param);
            return target.importFromWkt(getRemaWkt2());
        }
};

#endif  /* __remadem_mosaic_raster__ */