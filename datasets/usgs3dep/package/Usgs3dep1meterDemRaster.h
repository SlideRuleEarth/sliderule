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

#ifndef __usgs3dep_1meter_dem_raster__
#define __usgs3dep_1meter_dem_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoIndexedRaster.h"

/******************************************************************************
 * USGS3DEP 1METER DEM RASTER CLASS
 ******************************************************************************/

class Usgs3dep1meterDemRaster: public GeoIndexedRaster
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* URL_str;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static RasterObject* create(lua_State* L, RequestFields* rqst_parms, const char* key)
                          { return new Usgs3dep1meterDemRaster(L, rqst_parms, key); }


    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                Usgs3dep1meterDemRaster (lua_State* L, RequestFields* rqst_parms, const char* key);
               ~Usgs3dep1meterDemRaster (void) override;

        void    getIndexFile     (const OGRGeometry* geo, std::string& file) final;
        void    getIndexFile     (const std::vector<point_info_t>* points, std::string& file) final;
        bool    findRasters      (raster_finder_t* finder) final;

        static OGRErr overrideTargetCRS(OGRSpatialReference& target, const void* param=NULL);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/
        std::string filePath;
        std::string indexFile;
};

#endif  /* __usgs3dep_1meter_dem_raster__ */
