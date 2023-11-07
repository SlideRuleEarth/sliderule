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

#ifndef __atl06_reader__
#define __atl06_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "List.h"
#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "StringLib.h"

#include "H5Array.h"
#include "H5DArray.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * ATL06 READER
 ******************************************************************************/

class Atl06Reader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int MAX_NAME_STR = H5CORO_MAXIMUM_NAME_SIZE;
        static const int BATCH_SIZE = 256;

        static const char* elRecType;
        static const RecordObject::fieldDef_t elRecDef[];

        static const char* atRecType;
        static const RecordObject::fieldDef_t atRecDef[];

        static const char* ancFieldRecType;
        static const RecordObject::fieldDef_t ancFieldRecDef[];

        static const char* ancRecType;
        static const RecordObject::fieldDef_t ancRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Elevation Measurement */
        typedef struct {
            uint64_t        extent_id;              // unique identifier
            int64_t         time_ns;                // nanoseconds from GPS epoch
            uint32_t        segment_id;             // closest atl06 segment
            uint16_t        rgt;                    // reference ground track
            uint16_t        cycle;                  // cycle number
            uint8_t         spot;                   // 1 through 6, or 0 if unknown
            uint8_t         gt;                     // gt1l, gt1r, gt2l, gt2r, gt3l, gt3r
            int8_t          atl06_quality_summary;
            int8_t          bsnow_conf;
            int32_t         n_fit_photons;
            double          latitude;
            double          longitude;
            double          x_atc;
            double          y_atc;
            float           h_li;
            float           h_li_sigma;
            float           sigma_geo_h;
            float           seg_azimuth;
            float           dh_fit_dx;
            float           h_robust_sprd;
            float           w_surface_window_final;
            float           bsnow_h;
            float           r_eff;
            float           tide_ocean;
        } elevation_t;

        /* ATL06 Record */
        typedef struct {
            elevation_t     elevation[BATCH_SIZE];
        } atl06_t;

        /* Ancillary Record */
        typedef struct {
            uint64_t        extent_id;
            uint8_t         value[8];
        } anc_field_t;

        /* Ancillary Record */
        typedef struct {
            uint8_t         field_index; // position in request parameter list
            uint8_t         data_type; // RecordObject::fieldType_t
            anc_field_t     data[BATCH_SIZE];
        } anc_t;

        /* Statistics */
        typedef struct {
            uint32_t        segments_read;
            uint32_t        extents_filtered;
            uint32_t        extents_sent;
            uint32_t        extents_dropped;
            uint32_t        extents_retried;
        } stats_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef Dictionary<H5DArray*> H5DArrayDictionary;

        typedef struct {
            Atl06Reader*    reader;
            char            prefix[7];
            int             track;
            int             pair;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                explicit Region     (info_t* info);
                ~Region             (void);

                void cleanup        (void);
                void polyregion     (void);
                void rasterregion   (info_t* info);

                H5Array<double>     latitude;
                H5Array<double>     longitude;

                MathLib::point_t*   projected_poly;
                MathLib::proj_t     projection;
                int                 points_in_polygon;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_segment;
                long                num_segments;
        };

        /* Atl06 Data Subclass */
        class Atl06Data
        {
            public:

                Atl06Data           (info_t* info, const Region& region);
                ~Atl06Data          (void);

                /* Read Data */
                H5Array<int8_t>     sc_orient;
                H5Array<double>     delta_time;
                H5Array<float>      h_li;
                H5Array<float>      h_li_sigma;
                H5Array<int8_t>     atl06_quality_summary;
                H5Array<uint32_t>   segment_id;
                H5Array<float>      sigma_geo_h;
                H5Array<double>     x_atc;
                H5Array<double>     y_atc;
                H5Array<float>      seg_azimuth;
                H5Array<float>      dh_fit_dx;
                H5Array<float>      h_robust_sprd;
                H5Array<int32_t>    n_fit_photons;
                H5Array<float>      w_surface_window_final;
                H5Array<int8_t>     bsnow_conf;
                H5Array<float>      bsnow_h;
                H5Array<float>      r_eff;
                H5Array<float>      tide_ocean;

                H5DArrayDictionary  anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid[Icesat2Parms::NUM_SPOTS];
        Mutex               threadMut;
        int                 threadCount;
        int                 numComplete;
        Asset*              asset;
        const char*         resource;
        bool                sendTerminator;
        const int           read_timeout_ms;
        Publisher*          outQ;
        Icesat2Parms*       parms;
        stats_t             stats;

        H5Coro::context_t   context; // for ATL06 file

        int32_t             start_rgt;
        int32_t             start_cycle;
        int32_t             start_region;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl06Reader                 (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, Icesat2Parms* _parms, bool _send_terminator=true);
                            ~Atl06Reader                (void);

        static void*        subsettingThread            (void* parm);

        void                postRecord                  (RecordObject& record, stats_t& local_stats);
        static void         parseResource               (const char* resource, int32_t& rgt, int32_t& cycle, int32_t& region);

        static int          luaParms                    (lua_State* L);
        static int          luaStats                    (lua_State* L);
};

#endif  /* __atl06_reader__ */
