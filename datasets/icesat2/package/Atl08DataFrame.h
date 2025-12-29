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
         * Types
         *--------------------------------------------------------------------*/

        /* Atl08 Data Subclass */
        class Atl08Data
        {
            public:

                Atl08Data           (Atl08DataFrame* df, const AreaOfInterestT<float>& aoi);
                ~Atl08Data          (void) = default;

                H5Array<int8_t>     sc_orient;
                H5Array<double>     delta_time;
                H5Array<int32_t>    segment_id_beg;
                H5Array<int32_t>    segment_id_end;
                H5Array<uint8_t>    night_flag;
                H5Array<int32_t>    n_seg_ph;
                H5Array<float>      solar_elevation;
                H5Array<float>      solar_azimuth;
                H5Array<uint8_t>    terrain_flg;
                H5Array<uint8_t>    brightness_flag;
                H5Array<int8_t>     cloud_flag_atm;
                H5Array<float>      h_te_best_fit;
                H5Array<float>      h_te_interp;
                H5Array<float>      terrain_slope;
                H5Array<int32_t>    n_te_photons;
                H5Array<int8_t>     te_quality_score;
                H5Array<float>      h_te_uncertainty;
                H5Array<float>      h_canopy;
                H5Array<float>      h_canopy_abs;
                H5Array<float>      h_canopy_uncertainty;
                H5Array<int16_t>    segment_cover;
                H5Array<int32_t>    n_ca_photons;
                H5Array<int8_t>     can_quality_score;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<uint64_t>    extent_id;
        FieldColumn<time8_t>     time_ns            {Field::TIME_COLUMN};
        FieldColumn<double>      delta_time_col;
        FieldColumn<double>      latitude           {Field::Y_COLUMN};
        FieldColumn<double>      longitude          {Field::X_COLUMN};
        FieldColumn<int32_t>     segment_id_beg;
        FieldColumn<int32_t>     segment_id_end;
        FieldColumn<uint8_t>     night_flag;
        FieldColumn<int32_t>     n_seg_ph;
        FieldColumn<float>       solar_elevation;
        FieldColumn<float>       solar_azimuth;
        FieldColumn<uint8_t>     terrain_flg;
        FieldColumn<uint8_t>     brightness_flag;
        FieldColumn<int8_t>      cloud_flag_atm;
        FieldColumn<float>       h_te_best_fit      {Field::Z_COLUMN};
        FieldColumn<float>       h_te_interp;
        FieldColumn<float>       terrain_slope;
        FieldColumn<int32_t>     n_te_photons;
        FieldColumn<int8_t>      te_quality_score;
        FieldColumn<float>       h_te_uncertainty;
        FieldColumn<float>       h_canopy;
        FieldColumn<float>       h_canopy_abs;
        FieldColumn<float>       h_canopy_uncertainty;
        FieldColumn<int16_t>     segment_cover;
        FieldColumn<int32_t>     n_ca_photons;
        FieldColumn<int8_t>      can_quality_score;

        /* DataFrame MetaData */
        FieldElement<uint8_t>    spot;
        FieldElement<uint8_t>    cycle;
        FieldElement<uint8_t>    region;
        FieldElement<uint16_t>   rgt;
        FieldElement<uint8_t>    gt;
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
