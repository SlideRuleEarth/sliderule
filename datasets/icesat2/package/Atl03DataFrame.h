/*
 * Copyright (c) 2025, University of Washington
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

#ifndef __atl03_dataframe__
#define __atl03_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "GeoDataFrame.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "H5Array.h"
#include "H5VarSet.h"
#include "H5Object.h"
#include "Icesat2Fields.h"
#include "AreaOfInterest03.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/
class Atl03DataFrame: public GeoDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<time8_t>        time_ns             {Field::TIME_COLUMN, 0, "Photon timestamp (Unix ns)"};
        FieldColumn<double>         latitude            {Field::Y_COLUMN,    0, "Latitude (degrees)"};
        FieldColumn<double>         longitude           {Field::X_COLUMN,    0, "Longitude (degrees)"};
        FieldColumn<int32_t>        segment_id          {"ATL03 segment ID"};
        FieldColumn<double>         x_atc               {"Along-track distance (m)"};
        FieldColumn<float>          y_atc               {"Across-track distance (m)"};
        FieldColumn<float>          height              {Field::Z_COLUMN, 0, "Photon height WGS84 (m)"};
        FieldColumn<float>          solar_elevation     {"Solar elevation angle (deg)"};
        FieldColumn<float>          background_rate     {"Background photon rate (Hz)"};
        FieldColumn<float>          spacecraft_velocity {"Spacecraft velocity (m/s)"};
        FieldColumn<int8_t>         atl03_cnf           {"ATL03 confidence level"};
        FieldColumn<int8_t>         quality_ph          {"Photon quality flag"};
        FieldColumn<uint32_t>       ph_index            {"Photon index within segment"};

        FieldColumn<float>          relief              {"Local relief (m)"};
        FieldColumn<uint8_t>        landcover           {"Land cover classification"};
        FieldColumn<uint8_t>        snowcover           {"Snow cover flag"};
        FieldColumn<uint8_t>        atl08_class         {"ATL08 classification"};
        FieldColumn<uint16_t>       yapc_score          {"YAPC confidence score"};
        FieldColumn<uint8_t>        atl24_class         {"ATL24 classification (if enabled)"};
        FieldColumn<float>          atl24_confidence    {"ATL24 confidence (if enabled)"};

        /* DataFrame MetaData */
        FieldElement<uint8_t>       spot    {0, Field::META_COLUMN, "Spot number 1-6"};
        FieldElement<uint8_t>       cycle   {0, Field::META_COLUMN, "Orbital cycle"};
        FieldElement<uint8_t>       region  {0, Field::META_COLUMN, "ICESat-2 region number"};
        FieldElement<uint16_t>      rgt     {0, Field::META_COLUMN, "Reference Ground Track"};
        FieldElement<uint8_t>       gt      {0, Field::META_COLUMN, "Ground track (10,20,30,40,50,60)"};
        FieldElement<string>        granule;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Atl03 Data Subclass */
        class Atl03Data
        {
            public:

                Atl03Data           (Atl03DataFrame* df, const AreaOfInterest03& aoi);
                ~Atl03Data          (void) = default;

                H5Array<int8_t>     sc_orient;
                H5Array<float>      velocity_sc;
                H5Array<double>     segment_delta_time;
                H5Array<int32_t>    segment_id;
                H5Array<double>     segment_dist_x;
                H5Array<float>      solar_elevation;
                H5Array<int8_t>     podppd_flag;  // degraded POD/PPD
                H5Array<float>      dist_ph_along;
                H5Array<float>      dist_ph_across;
                H5Array<float>      h_ph;
                H5Array<int8_t>     signal_conf_ph;
                H5Array<int8_t>     quality_ph;
                H5Array<uint8_t>    weight006_ph; // yapc from release 006
                H5Array<uint16_t>   weight007_ph; // yapc from release 007
                H5Array<double>     lat_ph;
                H5Array<double>     lon_ph;
                H5Array<double>     delta_time;
                H5Array<double>     bckgrd_delta_time;
                H5Array<float>      bckgrd_rate;
                H5Array<float>      geoid;

                H5VarSet            anc_bckgrd_data;
                H5VarSet            anc_geo_data;
                H5VarSet            anc_corr_data;
                H5VarSet            anc_ph_data;
        };

        /* Atl08 Classification Subclass */
        class Atl08Class
        {
            public:

                /* Constants */
                static const int NUM_ATL03_SEGS_IN_ATL08_SEG = 5;
                static const uint8_t INVALID_FLAG = 0xFF;

                /* Methods */
                explicit Atl08Class (Atl03DataFrame* df);
                ~Atl08Class         (void);
                void classify       (const Atl03DataFrame* df, const AreaOfInterest03& aoi, const Atl03Data& atl03);
                uint8_t operator[]  (int index) const;

                /* Class Data */
                bool                enabled;
                bool                phoreal;
                bool                ancillary;

                /* Generated Data */
                uint8_t*            classification; // [num_photons]
                float*              relief;         // [num_photons]
                uint8_t*            landcover;      // [num_photons]
                uint8_t*            snowcover;      // [num_photons]

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
                H5VarSet            anc_seg_data;
                int32_t*            anc_seg_indices;
        };

        /* Atl24 Classification Subclass */
        class Atl24Class
        {
            public:

                /* Methods */
                explicit Atl24Class (Atl03DataFrame* df);
                ~Atl24Class         (void);
                void classify       (const Atl03DataFrame* df, const AreaOfInterest03& aoi, const Atl03Data& atl03);

                /* Class Data */
                bool                enabled;

                /* Generated Data */
                uint8_t*            classification; // [num_photons]
                float*              confidence;     // [num_photons]

                /* Read Data */
                H5Array<int32_t>    atl24_index_ph;
                H5Array<int8_t>     atl24_class_ph;
                H5Array<double>     atl24_confidence;
        };

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double ATL03_SEGMENT_LENGTH;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        std::atomic<bool>   active;
        Thread*             readerPid;
        const int           readTimeoutMs;
        int                 signalConfColIndex;
        const char*         beam;
        Publisher*          outQ;
        Icesat2Fields*      parms;
        H5Object*           hdf03;  // atl03 granule
        H5Object*           hdf08;  // atl08 granule
        H5Object*           hdf24;  // atl24 granule
        okey_t              dfKey;
        bool                usePodppd;
        bool                useYapc006;
        bool                useYapc007;
        bool                useGeoid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl03DataFrame      (lua_State* L, const char* beam_str, Icesat2Fields* _parms,
                                             H5Object* _hdf03, H5Object* _hdf08, H5Object* _hdf24,
                                             const char* outq_name);
                        ~Atl03DataFrame     (void) override;
        okey_t          getKey              (void) const override;
        static void*    subsettingThread    (void* parm);
};

#endif  /* __atl03_dataframe__ */
