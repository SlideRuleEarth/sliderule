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

#include "H5Array.h"
#include "H5DArray.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * ATL03 READER
 ******************************************************************************/

class Atl03Reader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int32_t INVALID_INDICE = -1;

        static const char* phRecType;
        static const RecordObject::fieldDef_t phRecDef[];

        static const char* exRecType;
        static const RecordObject::fieldDef_t exRecDef[];

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Photon Fields */
        typedef struct {
            int64_t         time_ns;        // nanoseconds since GPS epoch
            double          latitude;
            double          longitude;
            float           x_atc;          // float[]: dist_ph_along
            float           y_atc;          // float[]: dist_ph_across
            float           height;         // float[]: h_ph
            float           relief;         // float[]: ATL08 ph_h
            uint8_t         landcover;      // ATL08 land cover flags
            uint8_t         snowcover;      // ATL08 snow cover flags
            uint8_t         atl08_class;    // ATL08 classification
            int8_t          atl03_cnf;      // ATL03 confidence level
            int8_t          quality_ph;     // ATL03 photon quality
            uint8_t         yapc_score;     // YAPC weight of photon
        } photon_t;

        /* Extent Record */
        typedef struct {
            uint8_t         region;
            uint8_t         track; // 1, 2, or 3
            uint8_t         pair; // 0 (l), 1 (r)
            uint8_t         spacecraft_orientation; // sc_orient_t
            uint16_t        reference_ground_track;
            uint8_t         cycle;
            uint32_t        segment_id;
            uint32_t        photon_count;
            float           solar_elevation;
            float           spacecraft_velocity; // meters per second
            double          segment_distance;
            double          extent_length; // meters
            double          background_rate; // PE per second
            uint64_t        extent_id;
            photon_t        photons[]; // zero length field
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

        typedef Dictionary<H5DArray*> H5DArrayDictionary;

        typedef struct {
            Atl03Reader*    reader;
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
                void polyregion     (info_t* info);
                void rasterregion   (info_t* info);

                H5Array<double>     segment_lat;
                H5Array<double>     segment_lon;
                H5Array<int32_t>    segment_ph_cnt;

                bool*               inclusion_mask;
                bool*               inclusion_ptr;

                long                first_segment;
                long                num_segments;
                long                first_photon;
                long                num_photons;
        };

        /* Atl03 Data Subclass */
        class Atl03Data
        {
            public:

                Atl03Data           (info_t* info, const Region& region);
                ~Atl03Data          (void);

                /* Read Data */
                H5Array<int8_t>     sc_orient;
                H5Array<float>      velocity_sc;
                H5Array<double>     segment_delta_time;
                H5Array<int32_t>    segment_id;
                H5Array<double>     segment_dist_x;
                H5Array<float>      solar_elevation;
                H5Array<float>      dist_ph_along;
                H5Array<float>      dist_ph_across;
                H5Array<float>      h_ph;
                H5Array<int8_t>     signal_conf_ph;
                H5Array<int8_t>     quality_ph;
                H5Array<double>     lat_ph;
                H5Array<double>     lon_ph;
                H5Array<double>     delta_time;
                H5Array<double>     bckgrd_delta_time;
                H5Array<float>      bckgrd_rate;

                H5DArrayDictionary* anc_geo_data;
                H5DArrayDictionary* anc_ph_data;
        };

        /* Atl08 Classification Subclass */
        class Atl08Class
        {
            public:

                /* Constants */
                static const int NUM_ATL03_SEGS_IN_ATL08_SEG = 5;
                static const uint8_t INVALID_FLAG = 0xFF;

                /* Methods */
                explicit Atl08Class (const info_t* info);
                ~Atl08Class         (void);
                void classify       (const info_t* info, const Region& region, const Atl03Data& atl03);
                uint8_t operator[]  (int index) const;

                /* Class Data */
                bool                enabled;
                bool                phoreal;
                bool                ancillary;

                /* Generated Data */
                uint8_t*            classification; // [num_photons]
                float*              relief; // [num_photons]
                uint8_t*            landcover; // [num_photons]
                uint8_t*            snowcover; // [num_photons]

                /* Read Data */
                H5Array<int32_t>    atl08_segment_id;
                H5Array<int32_t>    atl08_pc_indx;
                H5Array<int8_t>     atl08_pc_flag;

                /* PhoREAL - Read Data */
                H5Array<float>      atl08_ph_h;
                H5Array<int32_t>    segment_id_beg;
                H5Array<int16_t>    segment_landcover;
                H5Array<int8_t>     segment_snowcover;

                /* Ancillary Data */
                H5DArrayDictionary* anc_seg_data;
                int32_t*            anc_seg_indices;
        };

        /* YAPC Score Subclass */
        class YapcScore
        {
            public:

                YapcScore           (info_t* info, const Region& region, const Atl03Data& atl03);
                ~YapcScore          (void);

                void yapcV2         (info_t* info, const Region& region, const Atl03Data& atl03);
                void yapcV3         (info_t* info, const Region& region, const Atl03Data& atl03);

                uint8_t operator[]  (int index) const;

                /* Generated Data */
                uint8_t*            score; // [num_photons]
        };

        /* Track State Subclass */
        class TrackState
        {
            public:

                int32_t         ph_in;              // photon index
                int32_t         seg_in;             // segment index
                int32_t         seg_ph;             // current photon index in segment
                int32_t         start_segment;      // used to set start_distance
                double          start_distance;     // distance to start of extent (in meters from equator)
                double          seg_distance;       // distance to start of atl03 segment
                double          start_seg_portion;  // portion of segment extent is starting from
                bool            track_complete;     // flag when track processing has finished
                int32_t         bckgrd_in;          // bckgrd index
                List<photon_t>  extent_photons;     // list of individual photons in extent
                int32_t         extent_segment;     // current segment extent is pulling photons from
                bool            extent_valid;       // flag for validity of extent (atl06 checks)
                double          extent_length;      // custom length of the extent (in meters)

                explicit TrackState (const Atl03Data& atl03);
                ~TrackState         (void);
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double ATL03_SEGMENT_LENGTH;

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
        char*               resource08;
        bool                sendTerminator;
        const int           read_timeout_ms;
        Publisher*          outQ;
        Icesat2Parms*       parms;
        int                 signalConfColIndex;
        stats_t             stats;

        H5Coro::context_t   context; // for ATL03 file
        H5Coro::context_t   context08; // for ATL08 file

        uint16_t             start_rgt;
        uint8_t              start_cycle;
        uint8_t              start_region;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03Reader                 (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, Icesat2Parms* _parms, bool _send_terminator=true);
                            ~Atl03Reader                (void) override;

        static void*        subsettingThread            (void* parm);

        static double       calculateBackground         (TrackState& state, const Atl03Data& atl03);
        uint32_t            calculateSegmentId          (const TrackState& state, const Atl03Data& atl03);
        void                generateExtentRecord        (uint64_t extent_id, const info_t* info, TrackState& state, const Atl03Data& atl03, vector<RecordObject*>& rec_list, int& total_size);
        static void         generateAncillaryRecords    (uint64_t extent_id, AncillaryFields::list_t* field_list, H5DArrayDictionary* field_dict, Icesat2Parms::anc_type_t type, List<int32_t>* indices, vector<RecordObject*>& rec_list, int& total_size);
        void                postRecord                  (RecordObject& record, stats_t& local_stats);
        static void         parseResource               (const char* resource, uint16_t& rgt, uint8_t& cycle, uint8_t& region);

        static int          luaParms                    (lua_State* L);
        static int          luaStats                    (lua_State* L);

        /* Unit Tests */
        friend class UT_Atl03Reader;
};

#endif  /* __atl03_reader__ */
