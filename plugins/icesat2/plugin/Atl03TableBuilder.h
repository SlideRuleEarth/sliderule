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

#ifndef __atl03_table_builder__
#define __atl03_table_builder__

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
#include "Icesat2Parms.h"
#include "BathyParms.h"

/******************************************************************************
 * ATL03 TABLE BUILDER
 ******************************************************************************/

class Atl03TableBuilder: public LuaObject
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
            int64_t         time_ns;                // nanoseconds since GPS epoch
            int32_t         index_ph;               // unique index of photon in granule
            float           geoid_corr_h;           // geoid corrected height of photon, calculated from h_ph and geoid
            double          latitude;
            double          longitude;
            double          x_ph;                   // the easting coordinate in meters of the photon for the given UTM zone
            double          y_ph;                   // the northing coordinate in meters of the photon for the given UTM zone 
            double          x_atc;                  // along track distance calculated from segment_dist_x and dist_ph_along 
            double          y_atc;                  // dist_ph_across
            float           sigma_along;            // along track aerial uncertainty
            float           sigma_across;           // across track aerial uncertainty
            float           ndwi;                   // normalized difference water index using HLS data
            uint8_t         yapc_score;
            uint8_t         max_signal_conf;        // maximum value in the atl03 confidence table
            int8_t          quality_ph;
        } photon_t;

        /* Extent Record */
        typedef struct {
            uint8_t         region;
            uint8_t         track;                  // 1, 2, or 3
            uint8_t         pair;                   // 0 (l), 1 (r)
            uint8_t         spacecraft_orientation; // sc_orient_t
            uint16_t        reference_ground_track;
            uint8_t         cycle;
            uint8_t         utm_zone;
            uint32_t        photon_count;
            float           solar_elevation;
            float           wind_v;                 // the wind speed at the center photon of the subsetted granule; calculated from met_u10m and met_v10m
            float           pointing_angle;         // angle of beam as measured from nadir (TBD - how to get this)
            double          background_rate;        // PE per second
            uint64_t        extent_id;
            photon_t        photons[];              // zero length field
        } extent_t;

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
            Atl03TableBuilder*  builder;
            char                prefix[7];
            int                 track;
            int                 pair;
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
                H5Array<double>     segment_dist_x;
                H5Array<float>      solar_elevation;
                H5Array<float>      dist_ph_along;
                H5Array<float>      dist_ph_across;
                H5Array<float>      h_ph;
                H5Array<int8_t>     signal_conf_ph;
                H5Array<int8_t>     quality_ph;
                H5Array<uint8_t>    weight_ph; // yapc
                H5Array<double>     lat_ph;
                H5Array<double>     lon_ph;
                H5Array<double>     delta_time;
                H5Array<double>     bckgrd_delta_time;
                H5Array<float>      bckgrd_rate;
        };

        /* Atl09 Subclass */
        class Atl09Class
        {
            public:

                explicit Atl09Class (info_t* info);
                ~Atl09Class         (void);

                /* Generated Data */
                bool                valid;

                /* Read Data */
                H5Array<float>      met_u10m;
                H5Array<float>      met_v10m;
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
        char*               resource09;
        bool                sendTerminator;
        const int           read_timeout_ms;
        Publisher*          outQ;
        BathyParms*         parms;

        H5Coro::context_t   context; // for ATL03 file
        H5Coro::context_t   context09; // for ATL08 file

        uint16_t             start_rgt;
        uint8_t              start_cycle;
        uint8_t              start_region;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            Atl03TableBuilder           (lua_State* L, Asset* _asset, const char* _resource, const char* outq_name, BathyParms* _parms, bool _send_terminator=true);
                            ~Atl03TableBuilder          (void);

        static void*        subsettingThread            (void* parm);

        static double       calculateBackground         (int32_t current_segment, int32_t& bckgrd_in, const Atl03Data& atl03);
        static void         parseResource               (const char* resource, uint16_t& rgt, uint8_t& cycle, uint8_t& region);
};

#endif  /* __atl03_table_builder__ */
