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

#ifndef __atl03_viewer__
#define __atl03_viewer__

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
 * ATL03 VIEWER
 ******************************************************************************/

class Atl03Viewer: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int32_t INVALID_INDICE = -1;

        static const char* segRecType;
        static const RecordObject::fieldDef_t segRecDef[];

        static const char* batchRecType;
        static const RecordObject::fieldDef_t batchRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Segment Fields */
        typedef struct {
            int64_t         time_ns;        // nanoseconds since GPS epoch
            uint64_t        extent_id;
            double          latitude;
            double          longitude;
            double          dist_x;         // segment_dist_x
            uint32_t        id;             // segment_id
            uint32_t        ph_cnt;         // segment_ph_cnt
        } segment_t;

        /* Extent Record */
        typedef struct {
            uint8_t         region;
            uint8_t         track; // 1, 2, or 3
            uint8_t         pair; // 0 (l), 1 (r)
            uint8_t         spot; // 1 to 6
            uint16_t        reference_ground_track;
            uint8_t         cycle;
            segment_t       segments[]; // zero length field
        } extent_t;

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

        typedef struct {
            Atl03Viewer*    reader;
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

                H5Array<double>     segment_lat;
                H5Array<double>     segment_lon;
                H5Array<int32_t>    segment_ph_cnt;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_segment;
                long                num_segments;
        };

        /* Atl03 Data Subclass */
        class Atl03Data
        {
            public:

                Atl03Data           (const info_t* info, const Region& region);
                ~Atl03Data          (void);

                /* Read Data */
                H5Array<int8_t>     sc_orient;
                H5Array<double>     segment_delta_time;
                H5Array<int32_t>    segment_id;
                H5Array<double>     segment_dist_x;
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/


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

        H5Coro::context_t   context; // for ATL03 file

        uint16_t             start_rgt;
        uint8_t              start_cycle;
        uint8_t              start_region;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Viewer                 (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, Icesat2Parms* _parms, bool _send_terminator=true);
                            ~Atl03Viewer                (void) override;

        static void*        subsettingThread            (void* parm);

        void                postRecord                  (RecordObject& record, stats_t& local_stats);
        static void         parseResource               (const char* resource, uint16_t& rgt, uint8_t& cycle, uint8_t& region);

        static int          luaStats                    (lua_State* L);

        /* Unit Tests */
        // friend class UT_Atl03Reader;
};

#endif  /* __atl03_viewer__ */
