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

#ifndef __geo_fields__
#define __geo_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <gdal.h>

#include "OsApi.h"
#include "LuaObject.h"
#include "Asset.h"
#include "FieldDictionary.h"
#include "FieldElement.h"
#include "FieldList.h"
#include "AssetField.h"
#include "TimeLib.h"

/* Error codes for raster Sampling, Subsetting (SS) */
#define SS_NO_ERRORS              0
#define SS_THREADS_LIMIT_ERROR    (1 << 0)
#define SS_MEMPOOL_ERROR          (1 << 1)
#define SS_OUT_OF_BOUNDS_ERROR    (1 << 2)
#define SS_READ_ERROR             (1 << 3)
#define SS_WRITE_ERROR            (1 << 4)
#define SS_SUBRASTER_ERROR        (1 << 5)
#define SS_INDEX_FILE_ERROR       (1 << 6)
#define SS_RESOURCE_LIMIT_ERROR   (1 << 7)

/******************************************************************************
 * CLASSES
 ******************************************************************************/

/*--------------------------------------------------------------------
* GeoFields
*--------------------------------------------------------------------*/
class GeoFields: public FieldDictionary
{
    public:

        /*--------------------------------------------------------------------
        * Constants
        *--------------------------------------------------------------------*/

        static const char* PARMS;
        static const char* DEFAULT_KEY;

        static const char* NEARESTNEIGHBOUR_ALGO_STR;
        static const char* BILINEAR_ALGO_STR;
        static const char* CUBIC_ALGO_STR;
        static const char* CUBICSPLINE_ALGO_STR;
        static const char* LANCZOS_ALGO_STR;
        static const char* AVERAGE_ALGO_STR;
        static const char* MODE_ALGO_STR;
        static const char* GAUSS_ALGO_STR;
        static const char* ZONALSTATS_ALGO_STR;

        /*--------------------------------------------------------------------
        * Typedefs
        *--------------------------------------------------------------------*/

        typedef struct GeoBBox {
            double lon_min {0.0};
            double lat_min {0.0};
            double lon_max {0.0};
            double lat_max {0.0};
        } bbox_t;

        typedef enum {
            NEARESTNEIGHBOUR_ALGO = static_cast<int>(GRIORA_NearestNeighbour),
            BILINEAR_ALGO         = static_cast<int>(GRIORA_Bilinear),
            CUBIC_ALGO            = static_cast<int>(GRIORA_Cubic),
            CUBICSPLINE_ALGO      = static_cast<int>(GRIORA_CubicSpline),
            LANCZOS_ALGO          = static_cast<int>(GRIORA_Lanczos),
            AVERAGE_ALGO          = static_cast<int>(GRIORA_Average),
            MODE_ALGO             = static_cast<int>(GRIORA_Mode),
            GAUSS_ALGO            = static_cast<int>(GRIORA_Gauss)
        } sampling_algo_t;

        /*--------------------------------------------------------------------
        * Data
        *--------------------------------------------------------------------*/

        FieldElement<sampling_algo_t>   sampling_algo {NEARESTNEIGHBOUR_ALGO};
        FieldElement<int>               sampling_radius {0};
        FieldElement<string>            t0;
        FieldElement<string>            t1;
        FieldElement<string>            tc; // closest time
        FieldElement<bool>              zonal_stats {false};
        FieldElement<bool>              flags_file {false};
        FieldElement<string>            url_substring;
        FieldElement<bool>              use_poi_time {false};
        FieldElement<string>            doy_range;
        FieldElement<bool>              sort_by_index {false};
        FieldElement<string>            proj_pipeline;
        FieldElement<bbox_t>            aoi_bbox;
        FieldElement<string>            catalog;
        FieldList<string>               bands;
        AssetField                      asset;

        bool                            filter_time;
        bool                            filter_doy_range;
        bool                            doy_keep_inrange;
        int                             doy_start;
        int                             doy_end;
        bool                            filter_closest_time;
        TimeLib::gmt_time_t             closest_time;
        TimeLib::gmt_time_t             start_time;
        TimeLib::gmt_time_t             stop_time;

        /*--------------------------------------------------------------------
        * Methods
        *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);
        void fromLua (lua_State* L, int index) override;

        GeoFields   (void);
        ~GeoFields  (void) override = default;

        static std::string  sserror2str (uint32_t error);
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const GeoFields::sampling_algo_t& v);
int convertToLua(lua_State* L, const GeoFields::sampling_algo_t& v);
void convertFromLua(lua_State* L, int index, GeoFields::sampling_algo_t& v);

string convertToJson(const GeoFields::bbox_t& v);
int convertToLua(lua_State* L, const GeoFields::bbox_t& v);
void convertFromLua(lua_State* L, int index, GeoFields::bbox_t& v);

inline uint32_t toEncoding(GeoFields::sampling_algo_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(GeoFields::bbox_t& v) { (void)v; return Field::USER; };

#endif  /* __geo_fields__ */
