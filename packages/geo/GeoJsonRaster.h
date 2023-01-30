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

#include "VrtRaster.h"

/******************************************************************************
 * GEOJSON RASTER CLASS
 ******************************************************************************/

class GeoJsonRaster: public VrtRaster
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int   RASTER_NODATA_VALUE = 200;
        static const int   RASTER_PIXEL_ON = 1;

        static const char* FILEDATA_KEY;
        static const char* FILELENGTH_KEY;
        static const char* BBOX_KEY;
        static const char* CELLSIZE_KEY;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int            luaCreate      (lua_State* L);
        static GeoJsonRaster* create         (lua_State* L, int index);

        bool                  includes       (double lon, double lat);
        virtual              ~GeoJsonRaster  (void);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

    protected:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                GeoJsonRaster   (lua_State *L, const char *image, long imagelength, double _cellsize);
        void    getRisFile      (std::string& file, double lon=0, double lat=0);
        bool    getRasterDate   (raster_info_t& rinfo);

    private:
        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        std::string rasterFile;
        std::string vrtFile;
        TimeLib::gmt_time_t gmtDate;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/
};

#endif  /* __geojson_raster__ */
