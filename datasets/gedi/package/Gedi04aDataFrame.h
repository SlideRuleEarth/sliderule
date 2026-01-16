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

#ifndef __gedi04a_dataframe__
#define __gedi04a_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "GediDataFrame.h"

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

class Gedi04aDataFrame: public GediDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        /* Gedi04a Data Subclass */
        class Gedi04aData
        {
            public:

                Gedi04aData      (Gedi04aDataFrame* df, const AreaOfInterestGedi& aoi);
                ~Gedi04aData     (void) = default;

                H5Array<uint64_t>   shot_number;
                H5Array<double>     delta_time;
                H5Array<float>      agbd;
                H5Array<float>      elev_lowestmode;
                H5Array<float>      solar_elevation;
                H5Array<float>      sensitivity;
                H5Array<uint8_t>    degrade_flag;
                H5Array<uint8_t>    l2_quality_flag;
                H5Array<uint8_t>    l4_quality_flag;
                H5Array<uint8_t>    surface_flag;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<uint64_t>   shot_number;
        FieldColumn<time8_t>    time_ns   {Field::TIME_COLUMN};
        FieldColumn<double>     latitude  {Field::Y_COLUMN};
        FieldColumn<double>     longitude {Field::X_COLUMN};
        FieldColumn<float>      agbd;
        FieldColumn<float>      elevation {Field::Z_COLUMN};
        FieldColumn<float>      solar_elevation;
        FieldColumn<float>      sensitivity;
        FieldColumn<uint8_t>    flags;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Gedi04aDataFrame  (lua_State* L, const char* beam_str, GediFields* _parms, H5Object* _hdf04a, const char* outq_name);
                        ~Gedi04aDataFrame (void) override = default;
        static void*    subsettingThread  (void* parm);
};

#endif  /* __gedi04a_dataframe__ */
