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

#ifndef __atl03_reader__
#define __atl03_reader__

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

#include "GTArray.h"
#include "GTDArray.h"
#include "lua_parms.h"

/******************************************************************************
 * ATL03 READER
 ******************************************************************************/

class Atl03Reader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int H5_READ_TIMEOUT_MS = 60000; // 60 seconds
        static const int MAX_NAME_STR = H5CORO_MAXIMUM_NAME_SIZE;

        static const char* phRecType;
        static const RecordObject::fieldDef_t phRecDef[];

        static const char* exRecType;
        static const RecordObject::fieldDef_t exRecDef[];

        static const char* ancRecType;
        static const RecordObject::fieldDef_t ancRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Photon Fields */
        typedef struct {
            double          delta_time; // seconds since ATLAS SDP epoch
            double          latitude;
            double          longitude;
            double          distance;   // double[]: dist_ph_along
            float           height;     // float[]: h_ph
            uint8_t         atl08_class;// ATL08 classification
            int8_t          atl03_cnf;  // ATL03 confidence level
            int8_t          quality_ph; // ATL03 photon quality
            uint8_t         yapc_score; // YAPC weight of photon
        } photon_t;

        /* Extent Record */
        typedef struct {
            bool            valid[PAIR_TRACKS_PER_GROUND_TRACK];
            uint8_t         reference_pair_track; // 1, 2, or 3
            uint8_t         spacecraft_orientation; // sc_orient_t
            uint16_t        reference_ground_track_start;
            uint16_t        cycle_start;
            uint64_t        extent_id;
            uint32_t        segment_id[PAIR_TRACKS_PER_GROUND_TRACK];
            double          segment_distance[PAIR_TRACKS_PER_GROUND_TRACK];
            double          extent_length[PAIR_TRACKS_PER_GROUND_TRACK]; // meters
            double          spacecraft_velocity[PAIR_TRACKS_PER_GROUND_TRACK]; // meters per second
            double          background_rate[PAIR_TRACKS_PER_GROUND_TRACK]; // PE per second
            uint32_t        photon_count[PAIR_TRACKS_PER_GROUND_TRACK];
            uint32_t        photon_offset[PAIR_TRACKS_PER_GROUND_TRACK];
            photon_t        photons[]; // zero length field
        } extent_t;

        /* Ancillary Extent Record */
        typedef struct {
        } extent_anc_t;

        /* Ancillary Record */
        typedef struct {
            uint64_t        extent_id;  // [RGT: 63-48][CYCLE: 47-32][RPT: 31-30][ID: 29-0]
            uint8_t         field_index; // position in request parameter list
            uint8_t         list_type; // ancillary_list_type_t
            uint8_t         data_type; // RecordObject::fieldType_t
            uint32_t        num_elements[PAIR_TRACKS_PER_GROUND_TRACK];
            uint8_t         data[];
        } atlxx_anc_t;

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
            Atl03Reader*    reader;
            int             track;
        } info_t;

        /* Region Subclass */
        class Region
        {
            public:

                Region              (info_t* info);
                ~Region             (void);

                void polyregion     (info_t* info);
                void rasterregion   (info_t* info);

                GTArray<double>     segment_lat;
                GTArray<double>     segment_lon;
                GTArray<int32_t>    segment_ph_cnt;

                bool*               inclusion_mask[PAIR_TRACKS_PER_GROUND_TRACK];
                bool*               inclusion_ptr[PAIR_TRACKS_PER_GROUND_TRACK];

                long                first_segment[PAIR_TRACKS_PER_GROUND_TRACK];
                long                num_segments[PAIR_TRACKS_PER_GROUND_TRACK];
                long                first_photon[PAIR_TRACKS_PER_GROUND_TRACK];
                long                num_photons[PAIR_TRACKS_PER_GROUND_TRACK];
        };

        /* Atl03 Data Subclass */
        class Atl03Data
        {
            public:

                Atl03Data           (info_t* info, Region* region);
                ~Atl03Data          (void);

                /* Read Data */
                GTArray<float>      velocity_sc;
                GTArray<double>     segment_delta_time;
                GTArray<int32_t>    segment_id;
                GTArray<double>     segment_dist_x;
                GTArray<float>      dist_ph_along;
                GTArray<float>      h_ph;
                GTArray<int8_t>     signal_conf_ph;
                GTArray<int8_t>     quality_ph;
                GTArray<double>     lat_ph;
                GTArray<double>     lon_ph;
                GTArray<double>     delta_time;
                GTArray<double>     bckgrd_delta_time;
                GTArray<float>      bckgrd_rate;

                MgDictionary<GTDArray*> anc_geolocation;
                MgDictionary<GTDArray*> anc_geocorrection;
                MgDictionary<GTDArray*> anc_height;
        };

        /* Atl08 Classification Subclass */
        class Atl08Class
        {
            public:

                Atl08Class          (info_t* info);
                ~Atl08Class         (void);
                void classify       (info_t* info, Region* region, Atl03Data* data);

                /* Class Data */
                bool                enabled;
                SafeString          resource;

                /* Generated Data */
                uint8_t*            gt[PAIR_TRACKS_PER_GROUND_TRACK];    // [num_photons]

                /* Read Data */
                GTArray<int32_t>    atl08_segment_id;
                GTArray<int32_t>    atl08_pc_indx;
                GTArray<int8_t>     atl08_pc_flag;

                MgDictionary<GTDArray*> anc_signal_photons;
        };

        /* YAPC Score Subclass */
        class YapcScore
        {
            public:

                YapcScore           (info_t* info, Region* region, Atl03Data* data);
                ~YapcScore          (void);

                void yapcV2         (info_t* info, Region* region, Atl03Data* data);
                void yapcV3         (info_t* info, Region* region, Atl03Data* data);

                /* Generated Data */
                uint8_t*            gt[PAIR_TRACKS_PER_GROUND_TRACK];    // [num_photons]
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double ATL03_SEGMENT_LENGTH;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                active;
        Thread*             readerPid[NUM_TRACKS];
        Mutex               threadMut;
        int                 threadCount;
        int                 numComplete;
        Asset*              asset;
        const char*         resource;
        const char*         resource08;
        bool                sendTerminator;
        Publisher*          outQ;
        icesat2_parms_t*    parms;
        stats_t             stats;

        H5Coro::context_t   context; // for ATL03 file
        H5Coro::context_t   context08; // for ATL08 file

        H5Array<int8_t>*    sc_orient;
        H5Array<int32_t>*   start_rgt;
        H5Array<int32_t>*   start_cycle;
        H5Array<int32_t>*   atl08_rgt; // pretest availability of atl08 file

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Reader             (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, icesat2_parms_t* _parms, bool _send_terminator=true);
                            ~Atl03Reader            (void);

        static void*        subsettingThread        (void* parm);

        bool                postRecord              (RecordObject* record, stats_t* local_stats);
        bool                sendAncillaryGeoRecords (uint64_t extent_id, ancillary_list_t* field_list, ancillary_list_type_t field_type, MgDictionary<GTDArray*>* field_dict, int32_t* start_element, stats_t* local_stats);

        static int          luaParms                (lua_State* L);
        static int          luaStats                (lua_State* L);

        /* Unit Tests */
        friend class UT_Atl03Reader;
};

#endif  /* __atl03_reader__ */
