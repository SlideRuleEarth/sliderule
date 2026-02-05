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

#ifndef __esa_copernicus_30meter_raster__
#define __esa_copernicus_30meter_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoRaster.h"

/******************************************************************************
 * ESA COPERNICUS 30 METER RASTER CLASS
 ******************************************************************************/

/*
 * Copernicus DEM (COP30) is an ESA Copernicus Global Digital Surface Model derived
 * from a multi-year collection of source datasets, predominantly the
 * TanDEM-X mission, with regional refinements from additional elevation sources.
 * As a compiled global product, it has no single acquisition date; temporal
 * provenance is defined by the Copernicus DEM product release (edition), and
 * SlideRule therefore uses the current product release date rather than a
 * collection midpoint. This dataset is distributed by OpenTopography and
 * hosted at the San Diego Supercomputer Center (SDSC) using an S3-compatible
 * object storage API. Although accessed via the S3 protocol, it is not hosted on Amazon S3.
 *
 * See: https://dataspace.copernicus.eu/explore-data/data-collections/copernicus-contributing-missions/collections-description/COP-DEM
 */

class EsaCopernicus30meterRaster: public GeoRaster
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static RasterObject* create(lua_State* L, RequestFields* rqst_parms, const char* key)
                          { return new EsaCopernicus30meterRaster(L, rqst_parms, key); }


    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        EsaCopernicus30meterRaster (lua_State* L, RequestFields* rqst_parms, const char* key):
         GeoRaster(L, rqst_parms, key,
                  rqst_parms->geoFields(key)->asset.asset->getIndex(),
                  TimeLib::datetime2gps(2023, 12, 15, 0, 0, 0) / 1000, /* Copernicus DEM Release 2023_1 (Dec 2023) */
                  1,                   /* elevationBandNum */
                  GdalRaster::NO_BAND, /* maskBandNum      */
                  NULL,                /* overrideGeoTransform */
                  overrideTargetCRS) {}

        static OGRErr overrideTargetCRS(OGRSpatialReference& target, const void* param=NULL)
        {
            static_cast<void>(param);
            return target.SetFromUserInput("EPSG:9055+3855");
        }
};

#endif  /* __esa_copernicus_30meter_raster__ */
