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

#ifndef __atl06_dataframe__
#define __atl06_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GeoDataFrame.h"
#include "H5VarSet.h"
#include "AreaOfInterest.h"
#include "Icesat2Fields.h"

using AreaOfInterest06 = AreaOfInterestT<double>;

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

class Atl06DataFrame: public GeoDataFrame
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
        class Atl06Data
        {
            public:

                Atl06Data           (Atl06DataFrame* df, const AreaOfInterest06& aoi);
                ~Atl06Data          (void) = default;

                H5Array<int8_t>     sc_orient;
                H5Array<double>     delta_time;
                H5Array<float>      h_li;
                H5Array<float>      h_li_sigma;
                H5Array<int8_t>     atl06_quality_summary;
                H5Array<uint32_t>   segment_id;
                H5Array<float>      sigma_geo_h;
                H5Array<double>     x_atc;
                H5Array<float>      y_atc;
                H5Array<float>      seg_azimuth;
                H5Array<float>      dh_fit_dx;
                H5Array<float>      h_robust_sprd;
                H5Array<int32_t>    n_fit_photons;
                H5Array<float>      w_surface_window_final;
                H5Array<int8_t>     bsnow_conf;
                H5Array<float>      bsnow_h;
                H5Array<float>      r_eff;
                H5Array<float>      tide_ocean;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<time8_t>     time_ns   {Field::TIME_COLUMN};
        FieldColumn<double>      latitude  {Field::Y_COLUMN};
        FieldColumn<double>      longitude {Field::X_COLUMN};
        FieldColumn<double>      x_atc;
        FieldColumn<float>       y_atc;
        FieldColumn<float>       h_li      {Field::Z_COLUMN};
        FieldColumn<float>       h_li_sigma;
        FieldColumn<float>       sigma_geo_h;
        FieldColumn<int8_t>      atl06_quality_summary;
        FieldColumn<uint32_t>    segment_id;
        FieldColumn<float>       seg_azimuth;
        FieldColumn<float>       dh_fit_dx;
        FieldColumn<float>       h_robust_sprd;
        FieldColumn<float>       w_surface_window_final;
        FieldColumn<int8_t>      bsnow_conf;
        FieldColumn<float>       bsnow_h;
        FieldColumn<float>       r_eff;
        FieldColumn<float>       tide_ocean;
        FieldColumn<int32_t>     n_fit_photons;

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
        H5Object*           hdf06;
        okey_t              dfKey;
        const char*         beam;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Atl06DataFrame  (lua_State* L, const char* beam_str, Icesat2Fields* _parms, H5Object* _hdf06, const char* outq_name);
                        ~Atl06DataFrame (void) override;
        okey_t          getKey          (void) const override;
        static void*    subsettingThread(void* parm);
};

#endif  /* __atl06_dataframe__ */
