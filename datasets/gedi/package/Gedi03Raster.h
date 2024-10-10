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

#ifndef __gedi03_raster__
#define __gedi03_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoRaster.h"

/******************************************************************************
 * GEDI 03 RASTER CLASS
 ******************************************************************************/

class Gedi03Raster: public GeoRaster
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void init (void) {}

        static RasterObject* create (lua_State* L, RequestFields* rqst_parms, const char* key)
        { return new Gedi03Raster(L, rqst_parms, key); }

    protected:


        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        Gedi03Raster(lua_State* L, RequestFields* rqst_parms, const char* key) :
         GeoRaster(L, rqst_parms, key,
                 std::string(rqst_parms->geoFields(key)->asset.asset->getPath()).append("/").append(rqst_parms->geoFields(key)->asset.asset->getIndex()),
                 TimeLib::datetime2gps(2022, 1, 19),
                 true /* Data is elevation */) {}
};

#endif  /* __gedi03_raster__ */
