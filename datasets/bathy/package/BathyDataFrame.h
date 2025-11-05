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
#include "H5Object.h"
#include "BathyFields.h"
#include "FieldElement.h"
#include "FieldArray.h"
#include "FieldColumn.h"
#include "GeoDataFrame.h"
#include "BathyMask.h"

#include <atomic>

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
        FieldElement<int>           track {0};          // ATL03 track (i.e. 1, 2, 3)
        FieldElement<int>           pair;               // ATL03 pair (i.e. left, right)
        FieldElement<int>           spot {0};           // ATL03 spot (1, 2, 3, 4, 5, 6)
        FieldElement<int>           utm_zone;
        FieldElement<bool>          utm_is_north;
        FieldList<double>           bounding_polygon_lat;
        FieldList<double>           bounding_polygon_lon;

        /* Column Data */
        FieldColumn<time8_t>        time_ns {Field::TIME_COLUMN}; // nanoseconds since GPS epoch
        FieldColumn<int32_t>        index_ph;           // unique index of photon in granule
        FieldColumn<int32_t>        index_seg;          // index into segment level groups in source ATL03 granule
        FieldColumn<double>         lat_ph {Field::Y_COLUMN}; // latitude of photon (EPSG 7912)
        FieldColumn<double>         lon_ph {Field::X_COLUMN}; // longitude of photon (EPSG 7912)
        FieldColumn<double>         x_ph;               // the easting coordinate in meters of the photon for the given UTM zone
        FieldColumn<double>         y_ph;               // the northing coordinate in meters of the photon for the given UTM zone
        FieldColumn<double>         x_atc;              // along track distance calculated from segment_dist_x and dist_ph_along
        FieldColumn<double>         y_atc;              // dist_ph_across
        FieldColumn<float>          ortho_h {Field::Z_COLUMN}; // refraction corrected, geoid corrected height of photon
        FieldColumn<float>          surface_h;          // orthometric height of sea surface at each photon location
        FieldColumn<float>          ellipse_h;          // height of photon with respect to reference ellipsoid
        FieldColumn<float>          sigma_thu;          // total horizontal uncertainty
        FieldColumn<float>          sigma_tvu;          // total vertical uncertainty
        FieldColumn<uint32_t>       processing_flags;   // bit mask of flags for capturing errors and warnings (top 8 bits reserved for classifiers)
        FieldColumn<int8_t>         max_signal_conf;    // maximum value in the atl03 confidence table
        FieldColumn<int8_t>         quality_ph;         // atl03 quality flags
        FieldColumn<int8_t>         class_ph;           // photon classification
        FieldColumn<float>          background_rate;    // PE per second
        FieldColumn<float>          geoid_corr_h;       // orthometric height without refraction correction (passed to classifiers)
        FieldColumn<float>          wind_v;             // wind speed (in meters/second)
        FieldColumn<float>          ref_el;             // reference elevation
        FieldColumn<float>          ref_az;             // reference aziumth
        FieldColumn<float>          sigma_across;       // across track aerial uncertainty
        FieldColumn<float>          sigma_along;        // along track aerial uncertainty
        FieldColumn<float>          sigma_h;            // vertical aerial uncertainty

        // temporary column so that python code can apply calculations on subaqueous photons
        FieldColumn<float>          refracted_dZ;
        FieldColumn<double>         refracted_lat;
        FieldColumn<double>         refracted_lon;
        FieldColumn<float>          subaqueous_sigma_thu;
        FieldColumn<float>          subaqueous_sigma_tvu;

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
                H5Array<double>     bounding_polygon_lat;
                H5Array<double>     bounding_polygon_lon;
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

        std::atomic<bool>           active;
        bool                        activeForAlerts;
        Thread*                     pid;
        BathyFields*                parmsPtr;
        const BathyFields&          parms;
        BathyMask*                  bathyMask;
        H5Object*                   hdf03;      // atl03 granule
        H5Object*                   hdf09;      // atl09 granule
        Publisher*                  rqstQ;
        int                         signalConfColIndex;
        int                         readTimeoutMs;
        bool                        valid;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                            BathyDataFrame              (lua_State* L, const char* beam_str, BathyFields* _parms, H5Object* _hdf03, H5Object* _hdf09, const char* rqstq_name, BathyMask* _mask);
                            ~BathyDataFrame             (void) override;

        static void*        subsettingThread            (void* parm);
        static float        calculateBackground         (int32_t current_segment, int32_t& bckgrd_in, const Atl03Data& atl03);

        static int          luaIsValid                  (lua_State* L);
        static int          luaLength                   (lua_State* L);

        #ifdef __unittesting__
            BathyDataFrame(BathyFields* _parms):
                GeoDataFrame(NULL, LUA_META_NAME, LUA_META_TABLE, {}, {}, Icesat2Fields::crsITRF2014_EGM08()),
                active(false),
                activeForAlerts(false),
                pid(NULL),
                parmsPtr(_parms),
                parms(*_parms),
                bathyMask(NULL),
                hdf03(NULL),
                hdf09(NULL),
                rqstQ(NULL),
                signalConfColIndex(0),
                readTimeoutMs(0),
                valid(false) {};
        #endif

        /*--------------------------------------------------------------------
         * Friends
         *--------------------------------------------------------------------*/

        friend class UT_BathyRefractionCorrector; // necessary for the private constructor/destructor
};

#endif  /* __bathy_data_frame__ */
