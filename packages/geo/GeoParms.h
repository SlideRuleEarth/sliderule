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

#ifndef __geo_parms__
#define __geo_parms__

/*
 * GeoParms works on batches of records.  It expects the `rec_type` passed
 * into the constructor to be the type that defines each of the column headings,
 * then it expects to receive records that are arrays (or batches) of that record
 * type.  The field defined as an array is transparent to this class - it just
 * expects the record to be a single array.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <gdal.h>

#include "OsApi.h"
#include "LuaObject.h"
#include "Asset.h"
#include "TimeLib.h"

/******************************************************************************
 * GEO PARAMETERS CLASS
 ******************************************************************************/

class GeoParms: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const char* SELF;
        static const char* SAMPLING_ALGO;
        static const char* SAMPLING_RADIUS;
        static const char* ZONAL_STATS;
        static const char* AUXILIARY_FILES;
        static const char* START_TIME;
        static const char* STOP_TIME;
        static const char* URL_SUBSTRING;
        static const char* CLOSEST_TIME;
        static const char* ASSET;

        static const char* NEARESTNEIGHBOUR_ALGO;
        static const char* BILINEAR_ALGO;
        static const char* CUBIC_ALGO;
        static const char* CUBICSPLINE_ALGO;
        static const char* LANCZOS_ALGO;
        static const char* AVERAGE_ALGO;
        static const char* MODE_ALGO;
        static const char* GAUSS_ALGO;
        static const char* ZONALSTATS_ALGO;

        static const char* OBJECT_TYPE;
        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        GDALRIOResampleAlg  sampling_algo;
        int                 sampling_radius;
        bool                zonal_stats;
        bool                auxiliary_files;
        bool                filter_time;
        TimeLib::gmt_time_t start_time;
        TimeLib::gmt_time_t stop_time;
        const char*         url_substring;
        bool                filter_closest_time;
        TimeLib::gmt_time_t closest_time;
        const char*         asset_name;
        Asset*              asset;

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
                    GeoParms    (lua_State* L, int index);
                    ~GeoParms   (void);

    private:

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        void                cleanup         (void);
        GDALRIOResampleAlg  str2algo        (const char* str);
        static int          luaAssetName    (lua_State* L);
        static int          luaAssetRegion  (lua_State* L);
};

#endif  /* __geo_parms__ */
