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

#ifndef __atl08_dataframe__
#define __atl08_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoDataFrame.h"
#include "H5VarSet.h"
#include "AreaOfInterest.h"
#include "Icesat2Fields.h"
#include "FieldArray.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

class Atl08DataFrame: public GeoDataFrame
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

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static constexpr int NUM_CANOPY_METRICS = 18;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Atl08 Data Subclass */
        class Atl08Data
        {
            public:

                Atl08Data           (Atl08DataFrame* df, const AreaOfInterest<float>& aoi);
                ~Atl08Data          (void) = default;

                H5Array<int8_t>     sc_orient;
                H5Array<double>     delta_time;
                H5Array<int32_t>    segment_id_beg;
                H5Array<uint8_t>    segment_landcover;
                H5Array<uint8_t>    segment_snowcover;
                H5Array<int32_t>    n_seg_ph;
                H5Array<float>      solar_elevation;
                H5Array<float>      terrain_slope;
                H5Array<int32_t>    n_te_photons;
                H5Array<int8_t>     te_quality_score;
                H5Array<float>      h_te_uncertainty;
                H5Array<float>      h_te_median;
                H5Array<float>      h_canopy;
                H5Array<float>      h_canopy_uncertainty;
                H5Array<int16_t>    segment_cover;
                H5Array<int32_t>    n_ca_photons;
                H5Array<int8_t>     can_quality_score;
                H5Array<float>      h_max_canopy;
                H5Array<float>      h_min_canopy;
                H5Array<float>      h_mean_canopy;
                H5Array<float>      canopy_openness;
                H5Array<float>      canopy_h_metrics;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<time8_t>     time_ns            {Field::TIME_COLUMN, 0, "Segment timestamp (Unix ns)"};
        FieldColumn<double>      latitude           {Field::Y_COLUMN,    0, "Latitude (degrees)"};
        FieldColumn<double>      longitude          {Field::X_COLUMN,    0, "Longitude (degrees)"};
        FieldColumn<int32_t>     segment_id_beg     {"Starting segment ID"};
        FieldColumn<int32_t>     n_seg_ph           {"Total photons in segment"};
        FieldColumn<float>       solar_elevation    {"Solar elevation angle (deg)"};
        FieldColumn<float>       terrain_slope      {"Terrain slope"};
        FieldColumn<int32_t>     n_te_photons       {"Terrain photon count"};
        FieldColumn<int8_t>      te_quality_score   {"Terrain quality score"};
        FieldColumn<float>       h_te_uncertainty   {"Terrain height uncertainty (m)"};
        FieldColumn<float>       h_canopy           {"Relative canopy height (m)"};
        FieldColumn<float>       h_canopy_uncertainty {"Canopy height uncertainty (m)"};
        FieldColumn<int16_t>     segment_cover      {"Canopy cover percentage"};
        FieldColumn<int32_t>     n_ca_photons       {"Canopy photon count"};
        FieldColumn<int8_t>      can_quality_score  {"Canopy quality score"};
        FieldColumn<uint8_t>     segment_landcover  {"Land cover classification"};
        FieldColumn<uint8_t>     segment_snowcover  {"Snow cover flag"};
        FieldColumn<float>       h_te_median        {Field::Z_COLUMN, 0, "Median terrain elevation (m)"};
        FieldColumn<float>       h_max_canopy       {"Max canopy height (m)"};
        FieldColumn<float>       h_min_canopy       {"Min canopy height (m)"};
        FieldColumn<float>       h_mean_canopy      {"Mean canopy height (m)"};
        FieldColumn<float>       canopy_openness    {"Canopy openness fraction"};
        FieldColumn<FieldArray<float,NUM_CANOPY_METRICS>> canopy_h_metrics {"18 canopy height percentiles from ATL08 HDF5"};

        /* DataFrame MetaData */
        FieldElement<uint8_t>    spot    {0, Field::META_COLUMN, "Spot number 1-6"};
        FieldElement<uint8_t>    cycle   {0, Field::META_COLUMN, "Orbital cycle"};
        FieldElement<uint8_t>    region  {0, Field::META_COLUMN, "ICESat-2 region number"};
        FieldElement<uint16_t>   rgt     {0, Field::META_COLUMN, "Reference Ground Track"};
        FieldElement<uint8_t>    gt      {0, Field::META_COLUMN, "Ground track (10,20,30,40,50,60)"};
        FieldElement<string>     granule;

        std::atomic<bool>   active;
        Thread*             readerPid;
        int                 readTimeoutMs;
        Publisher*          outQ;
        Icesat2Fields*      parms;
        H5Object*           hdf08;
        okey_t              dfKey;
        const char*         beam;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl08DataFrame  (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf08, const char* outq_name);
                        ~Atl08DataFrame (void) override;
        okey_t          getKey          (void) const override;
        static void*    subsettingThread(void* parm);
};

#endif  /* __atl08_dataframe__ */
