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

#ifndef __bathy_data_frame__
#define __bathy_data_frame__

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
#include "BathyFields.h"
#include "BathyClassifier.h"
#include "BathyRefractionCorrector.h"
#include "BathyUncertaintyCalculator.h"
#include "GeoDataFrame.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class BathyDataFrame: public GeoDataFrame
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
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct Info {
            BathyDataFrame*     reader;
            const BathyFields*  parms;
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

        /* Statistics SubClass */
        struct Stats
        {
            FieldElement<bool>      valid {true};
            FieldElement<uint64_t>  photonCount {0};
            FieldElement<uint64_t>  subaqueousPhotons {0.0};
            FieldElement<double>    correctionsDuration {0.0};
            FieldElement<double>    qtreesDuration {0.0};
            FieldElement<double>    coastnetDuration {0.0};
            FieldElement<double>    openoceansppDuration {0.0};
        };

        /* GranuleMetaData SubClass */
        struct GranuleMetaData: public FieldDictionary
        {
            FieldElement<int>       year;
            FieldElement<int>       month;
            FieldElement<int>       day;
            FieldElement<int>       rgt;
            FieldElement<int>       cycle;
            FieldElement<int>       region;
            FieldElement<int>       utm_zone;
            Stats                   stats;
            FieldElement<double>    atlas_sdp_gps_epoch;
            FieldElement<string>    data_end_utc;
            FieldElement<string>    data_start_utc;
            FieldElement<int32_t>   end_cycle;
            FieldElement<double>    end_delta_time;
            FieldElement<int32_t>   end_geoseg;
            FieldElement<double>    end_gpssow;
            FieldElement<int32_t>   end_gpsweek;
            FieldElement<int32_t>   end_orbit;
            FieldElement<int32_t>   end_region;
            FieldElement<int32_t>   end_rgt;
            FieldElement<string>    release;
            FieldElement<string>    granule_end_utc;
            FieldElement<string>    granule_start_utc;
            FieldElement<int32_t>   start_cycle;
            FieldElement<double>    start_delta_time;
            FieldElement<int32_t>   start_geoseg;
            FieldElement<double>    start_gpssow;
            FieldElement<int32_t>   start_gpsweek;
            FieldElement<int32_t>   start_orbit;
            FieldElement<int32_t>   start_region;
            FieldElement<int32_t>   start_rgt;
            FieldElement<string>    version;
            FieldElement<double>    crossing_time;
            FieldElement<int8_t>    cycle_number;
            FieldElement<double>    lan;
            FieldElement<int16_t>   orbit_number;
            FieldElement<int16_t>   rgt;
            FieldElement<int8_t>    sc_orient;
            FieldElement<double>    sc_orient_time;

            GranuleMetaData(void): FieldDictionary({}) {};
            ~GranuleMetaData(void) override = default;
        };

        /* SpotMetaData SubClass */
        struct SpotMetaData: public FieldDictionary??? // or does this just go into the metadata for the BeamDataFrame
        {
            FieldElement<int>       track;
            FieldElement<int>       pair;
            FieldElement<string>    beam;
            FieldElement<int>       spot;
            Stats                   stats;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                        active;
        Thread*                     readerPid[BathyFields::NUM_SPOTS];
        Mutex                       threadMut;
        int                         threadCount;
        int                         numComplete;
        BathyFields*                parms;
        BathyClassifier*            classifiers[BathyFields::NUM_CLASSIFIERS];
        BathyRefractionCorrector*   refraction;         // refraction correction
        BathyUncertaintyCalculator* uncertainty;        // uncertainty calculation
        GeoParms*                   hls;                // geo-package parms for sampling HLS for NDWI
        bool                        sendTerminator;
        Publisher*                  outQ;
        int                         signalConfColIndex;
        const char*                 sharedDirectory;
        int                         readTimeoutMs;
        H5Coro::Context*            context; // for ATL03 file
        H5Coro::Context*            context09; // for ATL09 file
        TimeLib::date_t             granuleDate;
        uint16_t                    startRgt;
        uint8_t                     startCycle;
        uint8_t                     startRegion;
        uint8_t                     sdpVersion;
        GeoLib::TIFFImage*          bathyMask;

        /* Meta Data */
        GranuleMetaData              granuleMetaData;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            BathyDataFrame              (lua_State* L,
                                                         BathyFields* _parms,
                                                         BathyClassifier** _classifiers,
                                                         BathyRefractionCorrector* _refraction,
                                                         BathyUncertaintyCalculator* _uncertainty,
                                                         GeoParms* _hls,
                                                         const char* outq_name,
                                                         const char* shared_directory,
                                                         bool read_sdp_variables,
                                                         bool _send_terminator);
                            ~BathyDataFrame             (void) override;

        static void*        subsettingThread            (void* parm);

        static double       calculateBackground         (int32_t current_segment, int32_t& bckgrd_in, const Atl03Data& atl03);
        static void         parseResource               (const char* resource, TimeLib::date_t& date,
                                                         uint16_t& rgt, uint8_t& cycle, uint8_t& region,
                                                         uint8_t& version);

        void                readStandardData            (GranuleMetaData& granule_meta_data, H5Coro::Context* context, int timeout)
        void                findSeaSurface              (BathyFields::extent_t& extent);
        void                writeCSV                    (const vector<BathyFields::extent_t*>& extents, int spot, const BathyStats& local_stats);

};

#endif  /* __bathy_data_frame__ */
