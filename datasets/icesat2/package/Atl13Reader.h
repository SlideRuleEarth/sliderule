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

#ifndef __atl13_reader__
#define __atl13_reader__

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

class Atl13Reader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int BATCH_SIZE = 256;

        static const char* wtRecType;
        static const RecordObject::fieldDef_t wtRecDef[];

        static const char* atRecType;
        static const RecordObject::fieldDef_t atRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Water Measurement */
        typedef struct {
            uint64_t        extent_id;              // unique identifier
            int64_t         time_ns;                // nanoseconds from GPS epoch
            uint32_t        segment_id;             // closest atl06 segment
            uint16_t        rgt;                    // reference ground track
            uint16_t        cycle;                  // cycle number
            uint8_t         spot;                   // 1 through 6, or 0 if unknown
            uint8_t         gt;                     // gt1l, gt1r, gt2l, gt2r, gt3l, gt3r
            int8_t          snow_ice_atl09;
            int8_t          cloud_flag_asr_atl09;
            double          latitude;
            double          longitude;
            float           ht_ortho;
            float           ht_water_surf;
            float           segment_azimuth;
            int32_t         segment_quality;
            float           segment_slope_trk_bdy;
            float           water_depth;
        } water_t;

        /* ATL13 Record */
        typedef struct {
            water_t         water[BATCH_SIZE];
        } atl13_t;

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
            Atl13Reader*    reader;
            char            prefix[7];
            int             track;
            int             pair;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                explicit Region     (const info_t* info);
                ~Region             (void);

                void cleanup        (void);
                void polyregion     (const info_t* info);
                void rasterregion   (const info_t* info);

                H5Array<double>     latitude;
                H5Array<double>     longitude;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_segment;
                long                num_segments;
        };

        /* Atl13 Data Subclass */
        class Atl13Data
        {
            public:

                Atl13Data           (const info_t* info, const Region& region);
                ~Atl13Data          (void);

                /* Read Data */
                H5Array<int8_t>     sc_orient;
                H5Array<double>     delta_time;
                H5Array<int32_t>    segment_id_beg;
                H5Array<int8_t>     snow_ice_atl09;
                H5Array<int8_t>     cloud_flag_asr_atl09;
                H5Array<float>      ht_ortho;
                H5Array<float>      ht_water_surf;
                H5Array<float>      segment_azimuth;
                H5Array<int32_t>    segment_quality;
                H5Array<float>      segment_slope_trk_bdy;
                H5Array<float>      water_depth;

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

        H5Coro::Context*    context; // for ATL13 file

        int32_t             start_rgt;
        int32_t             start_cycle;
        int32_t             start_region;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl13Reader                 (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, Icesat2Parms* _parms, bool _send_terminator=true);
                            ~Atl13Reader                (void) override;

        static void*        subsettingThread            (void* parm);

        void                postRecord                  (RecordObject& record, stats_t& local_stats);
        static void         parseResource               (const char* resource, int32_t& rgt, int32_t& cycle, int32_t& region);

        static int          luaStats                    (lua_State* L);
};

#endif  /* __atl13_reader__ */
