/*
 * Copyright (c) 2023, University of Texas
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
 * 3. Neither the name of the University of Texas nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __phoreal__
#define __phoreal__

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"
#include "FieldColumn.h"
#include "FieldArray.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class PhoReal: public GeoDataFrame::FrameRunner
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int NUM_PERCENTILES = 20;
        static const int MAX_BINS = 1000;

        static const double PercentileInterval[NUM_PERCENTILES];

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate   (lua_State* L);
        bool            run         (GeoDataFrame* dataframe) override;

    private:

         /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct result_t {
            uint16_t    pflags = 0;
            time8_t     time_ns {0};
            double      latitude = 0;
            double      longitude = 0;
            double      x_atc = 0;
            double      y_atc = 0;
            uint32_t    ground_photon_count = 0;
            uint32_t    vegetation_photon_count = 0;
            float       h_te_median = 0;
            float       h_max_canopy = 0;
            float       h_min_canopy = 0;
            float       h_mean_canopy = 0;
            float       h_canopy = 0;
            float       canopy_openness = 0;
            FieldArray<float,NUM_PERCENTILES> canopy_h_metrics;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        PhoReal             (lua_State* L, Icesat2Fields* _parms);
                        ~PhoReal            (void) override;

        void            geolocate           (const Atl03DataFrame& df, uint32_t start_photon, uint32_t num_photons, result_t& result);
        void            algorithm           (const Atl03DataFrame& df, uint32_t start_photon, uint32_t num_photons, result_t& result);
        static void     quicksort           (uint32_t* index_array, const FieldColumn<float>& column, int start, int end);
        static int      quicksortpartition  (uint32_t* index_array, const FieldColumn<float>& column, int start, int end);

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        static bool isVegetation (uint8_t atl08_class)
        {
            return (atl08_class == Icesat2Fields::ATL08_CANOPY || atl08_class == Icesat2Fields::ATL08_TOP_OF_CANOPY);
        }

        static bool isGround (uint8_t atl08_class)
        {
            return (atl08_class == Icesat2Fields::ATL08_GROUND);
        }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Icesat2Fields*  parms;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

 inline FieldUntypedColumn::column_t toDoubles(const FieldColumn<FieldArray<float,PhoReal::NUM_PERCENTILES>>& v, long start_index, long num_elements) {
    const long total_elements = num_elements * PhoReal::NUM_PERCENTILES;
    FieldUntypedColumn::column_t column = {
        .data = new double[total_elements],
        .size = total_elements
    };
    long index = 0;
    for(long i = start_index; i < (start_index + num_elements); i++) {
        for(long j = 0; j < PhoReal::NUM_PERCENTILES; j++) {
            column.data[index++] = static_cast<double>(v[i][j]);
        }
    }
    return column;
}

#endif