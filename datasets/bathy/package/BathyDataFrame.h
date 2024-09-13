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

#include "LuaObject.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "GeoLib.h"
#include "H5Array.h"
#include "H5Element.h"
#include "BathyFields.h"
#include "FieldElement.h"
#include "FieldArray.h"
#include "FieldColumn.h"
#include "GeoDataFrame.h"
#include "BathyStats.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class BathyDataFrame: public GeoDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* Meta Data */
        FieldElement<string>        beam;               // ATL03 beam (i.e. gt1l, gt1r, gt2l, gt2r, gt3l, gt3r)
        FieldElement<int>           track;              // ATL03 track (i.e. 1, 2, 3)
        FieldElement<int>           pair;               // ATL03 pair (i.e. left, right)
        FieldElement<int>           spot;               // ATL03 spot (1, 2, 3, 4, 5, 6)
        FieldElement<int>           utm_zone;
        FieldElement<bool>          utm_is_north;

        /* Column Data */
        FieldColumn<int64_t>        time_ns;            // nanoseconds since GPS epoch
        FieldColumn<int32_t>        index_ph;           // unique index of photon in granule
        FieldColumn<int32_t>        index_seg;          // index into segment level groups in source ATL03 granule
        FieldColumn<double>         lat_ph;             // latitude of photon (EPSG 7912)
        FieldColumn<double>         lon_ph;             // longitude of photon (EPSG 7912)
        FieldColumn<double>         x_ph;               // the easting coordinate in meters of the photon for the given UTM zone
        FieldColumn<double>         y_ph;               // the northing coordinate in meters of the photon for the given UTM zone
        FieldColumn<double>         x_atc;              // along track distance calculated from segment_dist_x and dist_ph_along
        FieldColumn<double>         y_atc;              // dist_ph_across
        FieldColumn<double>         background_rate;    // PE per second
        FieldColumn<float>          delta_h;            // refraction correction of height
        FieldColumn<float>          surface_h;          // orthometric height of sea surface at each photon location
        FieldColumn<float>          ortho_h;            // geoid corrected height of photon, calculated from h_ph and geoid
        FieldColumn<float>          ellipse_h;          // height of photon with respect to reference ellipsoid
        FieldColumn<float>          sigma_thu;          // total horizontal uncertainty
        FieldColumn<float>          sigma_tvu;          // total vertical uncertainty
        FieldColumn<uint32_t>       processing_flags;   // bit mask of flags for capturing errors and warnings
        FieldColumn<uint8_t>        yapc_score;         // atl03 density estimate (Yet Another Photon Classifier)
        FieldColumn<int8_t>         max_signal_conf;    // maximum value in the atl03 confidence table
        FieldColumn<int8_t>         quality_ph;         // atl03 quality flags
        FieldColumn<int8_t>         class_ph;           // photon classification
        FieldColumn<FieldArray<int8_t, BathyFields::NUM_CLASSIFIERS>> predictions; // photon classification from each of the classifiers
        FieldColumn<float>          wind_v;             // wind speed (in meters/second)            
        FieldColumn<float>          ref_el;             // reference elevation
        FieldColumn<float>          ref_az;             // reference aziumth
        FieldColumn<float>          sigma_across;       // across track aerial uncertainty
        FieldColumn<float>          sigma_along;        // along track aerial uncertainty
        FieldColumn<float>          sigma_h;            // vertical aerial uncertainty

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Region Subclass */
        class Region
        {
            public:

                explicit Region     (const BathyDataFrame& dataframe);
                ~Region             (void);

                void cleanup        (void);
                void polyregion     (const BathyDataFrame& dataframe);
                void rasterregion   (const BathyDataFrame& dataframe);

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

                Atl03Data           (const BathyDataFrame& dataframe, const Region& region);
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

                explicit Atl09Class (const BathyDataFrame& dataframe);
                ~Atl09Class         (void) = default;

                /* Generated Data */
                bool                valid;

                /* Read Data */
                H5Array<float>      met_u10m;
                H5Array<float>      met_v10m;
                H5Array<double>     delta_time;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                        active;
        Thread*                     pid;
        BathyFields*                parmsPtr;
        const BathyFields&          parms;
        BathyMask*                  bathyMask;
        H5Object*                   hdf03;      // atl03 granule
        H5Object*                   hdf09;      // atl09 granule
        H5Object*                   atl03File;
        Publisher                   rqstQ;
        int                         signalConfColIndex;
        int                         readTimeoutMs;
        int                         sdpVersion;
        bool                        valid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            BathyDataFrame              (lua_State* L, const string& beam_str, BathyFields* _parms, H5Object* _hdf03, H5Object* _hdf09, const char* rqstq_name, BathyMask* _mask);
                            ~BathyDataFrame             (void) override;

        static void*        subsettingThread            (void* parm);
        static double       calculateBackground         (int32_t current_segment, int32_t& bckgrd_in, const Atl03Data& atl03);
        static void         getResourceVersion          (const char* resource, int& version);
        void                findSeaSurface              (void);

        static int          luaIsValid                  (lua_State* L);
};

#endif  /* __bathy_data_frame__ */
