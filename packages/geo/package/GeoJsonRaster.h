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

#ifndef __geojson_raster__
#define __geojson_raster__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaEngine.h"
#include "GeoRaster.h"
#include "GeoFields.h"

/******************************************************************************
 * GEOJSON RASTER CLASS
 ******************************************************************************/

class GeoJsonRaster: public GeoRaster
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   RASTER_NODATA_VALUE = 200;
        static const int   RASTER_PIXEL_ON = 1;

        static const char* FILEDATA_KEY;
        static const char* BBOX_KEY;
        static const char* CELLSIZE_KEY;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* import bbox_t into this namespace from GeoFields.h */
        using bbox_t=GeoFields::bbox_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int            luaCreate      (lua_State* L);
        static GeoJsonRaster* create         (const string& geojson, double cellsize);

        bool                  includes       (double lon, double lat, double height=0);
                             ~GeoJsonRaster  (void) override;
        const char*           getJsonString  (void) { return geojstr; }

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        bool rawPixel (const uint32_t row, const uint32_t col)
        {
            return static_cast<int>(data[(row * cols) + col]) == RASTER_PIXEL_ON;
        }

        uint8_t* getRasterData (void) { return data; }
        uint32_t getRasterCols (void) const { return cols; }
        uint32_t getRasterRows (void) const { return rows; }
        bbox_t   getRasterBbox (void) const { return bbox; }


    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        GeoJsonRaster(lua_State* L, RequestFields* rqst_parms, const char* key, const char* _geojstr, double cellsize);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

         std::string rasterFileName;
         const char* geojstr;
         uint8_t*    data;
         double      cellsize;
         uint32_t    cols;
         uint32_t    rows;
         bbox_t      bbox;
};

#endif  /* __geojson_raster__ */
