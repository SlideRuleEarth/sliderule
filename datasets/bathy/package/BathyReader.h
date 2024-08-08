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

#ifndef __bathy_reader__
#define __bathy_reader__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "LuaObject.h"
#include "RecordObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "RasterObject.h"
#include "H5Array.h"
#include "H5Element.h"
#include "GeoLib.h"
#include "BathyParms.h"
#include "BathyClassifier.h"
#include "BathyRefractionCorrector.h"
#include "BathyUncertaintyCalculator.h"

/******************************************************************************
 * BATHY READER
 ******************************************************************************/

class BathyReader: public LuaObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int32_t INVALID_INDICE = -1;

        static const char* OUTPUT_FILE_PREFIX;

        static const char* GLOBAL_BATHYMETRY_MASK_FILE_PATH;
        static const double GLOBAL_BATHYMETRY_MASK_MAX_LAT;
        static const double GLOBAL_BATHYMETRY_MASK_MIN_LAT;
        static const double GLOBAL_BATHYMETRY_MASK_MAX_LON;
        static const double GLOBAL_BATHYMETRY_MASK_MIN_LON;
        static const double GLOBAL_BATHYMETRY_MASK_PIXEL_SIZE;
        static const uint32_t GLOBAL_BATHYMETRY_MASK_OFF_VALUE;

        static const char* OBJECT_TYPE;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Statitics */
        typedef struct {
            uint64_t    photon_count;
            uint64_t    subaqueous_photons;
            double      corrections_duration;
            double      qtrees_duration;
            double      coastnet_duration;
            double      openoceanspp_duration;
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

        typedef struct Info {
            BathyReader*        reader;
            const BathyParms*   parms;
            char                prefix[7];
            int                 track;
            int                 pair;
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
                long                first_photon;
                long                num_photons;
        };

        /* Atl03 Data Subclass */
        class Atl03Data
        {
            public:

                Atl03Data           (const info_t* info, const Region& region);
                ~Atl03Data          (void) = default;

                /* Read Data */
                H5Array<int8_t>     sc_orient;
                H5Array<float>      velocity_sc;
                H5Array<double>     segment_delta_time;
                H5Array<double>     segment_dist_x;
                H5Array<float>      solar_elevation;
                H5Array<float>      sigma_h;
                H5Array<float>      sigma_along;
                H5Array<float>      sigma_across;
                H5Array<float>      ref_azimuth;
                H5Array<float>      ref_elev;
                H5Array<float>      geoid;
                H5Array<float>      dem_h;
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

                explicit Atl09Class (const info_t* info);
                ~Atl09Class         (void) = default;

                /* Generated Data */
                bool                valid;

                /* Read Data */
                H5Array<float>      met_u10m;
                H5Array<float>      met_v10m;
                H5Array<double>     delta_time;
        };

        /* AncillaryData SubClass */
        class AncillaryData
        {
            public:

                explicit AncillaryData  (H5Coro::Context* context, int timeout);
                ~AncillaryData          (void) = default;
                const char* tojson      (void) const;

                H5Element<double>       atlas_sdp_gps_epoch;
                H5Element<const char*>  data_end_utc;
                H5Element<const char*>  data_start_utc;
                H5Element<int32_t>      end_cycle;
                H5Element<double>       end_delta_time;
                H5Element<int32_t>      end_geoseg;
                H5Element<double>       end_gpssow;
                H5Element<int32_t>      end_gpsweek;
                H5Element<int32_t>      end_orbit;
                H5Element<int32_t>      end_region;
                H5Element<int32_t>      end_rgt;
                H5Element<const char*>  release;
                H5Element<const char*>  granule_end_utc;
                H5Element<const char*>  granule_start_utc;
                H5Element<int32_t>      start_cycle;
                H5Element<double>       start_delta_time;
                H5Element<int32_t>      start_geoseg;
                H5Element<double>       start_gpssow;
                H5Element<int32_t>      start_gpsweek;
                H5Element<int32_t>      start_orbit;
                H5Element<int32_t>      start_region;
                H5Element<int32_t>      start_rgt;
                H5Element<const char*>  version;
        };

        /* OrbitInfo SubClass */
        class OrbitInfo
        {
            public:

                explicit OrbitInfo  (H5Coro::Context* context, int timeout);
                ~OrbitInfo          (void) = default;
                const char* tojson  (void) const;

                H5Element<double>   crossing_time;
                H5Element<int8_t>   cycle_number;
                H5Element<double>   lan;
                H5Element<int16_t>  orbit_number;
                H5Element<int16_t>  rgt;
                H5Element<int8_t>   sc_orient;
                H5Element<double>   sc_orient_time;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                        active;
        Thread*                     readerPid[BathyParms::NUM_SPOTS];
        Mutex                       threadMut;
        int                         threadCount;
        int                         numComplete;
        BathyParms*                 parms;
        BathyClassifier*            classifiers[BathyParms::NUM_CLASSIFIERS];
        BathyRefractionCorrector*   refraction;         // refraction correction
        BathyUncertaintyCalculator* uncertainty;        // uncertainty calculation
        GeoParms*                   hls;                // geo-package parms for sampling HLS for NDWI
        bool                        sendTerminator;
        Publisher*                  outQ;
        int                         signalConfColIndex;
        const char*                 sharedDirectory;
        int                         readTimeoutMs;
        stats_t                     stats;

        H5Coro::Context*            context; // for ATL03 file
        H5Coro::Context*            context09; // for ATL09 file

        TimeLib::date_t             granuleDate;

        uint16_t                    startRgt;
        uint8_t                     startCycle;
        uint8_t                     startRegion;
        uint8_t                     sdpVersion;

        GeoLib::TIFFImage*          bathyMask;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            BathyReader                 (lua_State* L,
                                                         BathyParms* _parms,
                                                         BathyClassifier** _classifiers,
                                                         BathyRefractionCorrector* _refraction,
                                                         BathyUncertaintyCalculator* _uncertainty,
                                                         GeoParms* _hls,
                                                         const char* outq_name,
                                                         const char* shared_directory,
                                                         bool _send_terminator);
                            ~BathyReader                (void) override;

        static void*        subsettingThread            (void* parm);

        static double       calculateBackground         (int32_t current_segment, int32_t& bckgrd_in, const Atl03Data& atl03);
        static void         parseResource               (const char* resource, TimeLib::date_t& date, 
                                                         uint16_t& rgt, uint8_t& cycle, uint8_t& region, 
                                                         uint8_t& version);
        
        void                findSeaSurface              (BathyParms::extent_t& extent);
        void                writeCSV                    (const vector<BathyParms::extent_t*>& extents, int spot, const stats_t& local_stats);

        static int          luaSpotEnabled              (lua_State* L);
        static int          luaClassifierEnabled        (lua_State* L);
        static int          luaStats                    (lua_State* L);

};

#endif  /* __bathy_reader__ */
