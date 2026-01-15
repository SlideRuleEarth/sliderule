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

#ifndef __gedi01b_dataframe__
#define __gedi01b_dataframe__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <atomic>

#include "GeoDataFrame.h"
#include "H5Array.h"
#include "H5Coro.h"
#include "H5Object.h"
#include "H5VarSet.h"
#include "StringLib.h"
#include "GediFields.h"
#include "AreaOfInterest.h"

using AreaOfInterestGedi = AreaOfInterestT<double>;

/******************************************************************************
 * CLASS DEFINITION
 ******************************************************************************/

class Gedi01bDataFrame: public GeoDataFrame
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int GEDI01B_TX_SAMPLES_MAX = 128;
        static const int GEDI01B_RX_SAMPLES_MAX = 2048;

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

        /* Gedi01b Data Subclass */
        class Gedi01bData
        {
            public:

                Gedi01bData      (Gedi01bDataFrame* df, const AreaOfInterestGedi& aoi);
                ~Gedi01bData     (void) = default;

                H5Array<uint64_t>   shot_number;
                H5Array<double>     delta_time;
                H5Array<double>     elev_bin0;
                H5Array<double>     elev_lastbin;
                H5Array<float>      solar_elevation;
                H5Array<uint8_t>    degrade_flag;
                H5Array<uint16_t>   tx_sample_count;
                H5Array<uint64_t>   tx_start_index;
                H5Array<uint16_t>   rx_sample_count;
                H5Array<uint64_t>   rx_start_index;

                H5VarSet            anc_data;
        };

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        /* DataFrame Columns */
        FieldColumn<uint64_t>           shot_number;
        FieldColumn<time8_t>            time_ns   {Field::TIME_COLUMN};
        FieldColumn<double>             latitude  {Field::Y_COLUMN};
        FieldColumn<double>             longitude {Field::X_COLUMN};
        FieldColumn<float>              elevation_start {Field::Z_COLUMN};
        FieldColumn<double>             elevation_stop;
        FieldColumn<double>             solar_elevation;
        FieldColumn<uint16_t>           tx_size;
        FieldColumn<uint16_t>           rx_size;
        FieldColumn<uint8_t>            flags;
        FieldColumn<FieldList<float>>   tx_waveform;
        FieldColumn<FieldList<float>>   rx_waveform;

        /* DataFrame MetaData */
        FieldElement<uint8_t>           beam;
        FieldElement<uint32_t>          orbit;
        FieldElement<uint16_t>          track;
        FieldElement<string>            granule;

        std::atomic<bool>   active;
        Thread*             readerPid;
        int                 readTimeoutMs;
        Publisher*          outQ;
        GediFields*         parms;
        H5Object*           hdf01b;
        okey_t              dfKey;
        const char*         beamStr;
        char                group[9];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        Gedi01bDataFrame (lua_State* L, const char* beam_str, GediFields* _parms, H5Object* _hdf01b, const char* outq_name);
                        ~Gedi01bDataFrame (void) override;
        okey_t          getKey          (void) const override;
        static void*    subsettingThread(void* parm);
};

#endif  /* __gedi01b_dataframe__ */
