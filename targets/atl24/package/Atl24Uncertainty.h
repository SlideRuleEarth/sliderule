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

#ifndef __atl24_uncertainty__
#define __atl24_uncertainty__

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "H5CoroLib.h"
#include "H5Array.h"
#include "BathyParameters.h"
#include "BathyDataFrame.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class Atl24Uncertainty: public GeoDataFrame::FrameRunner
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

        static int      luaCreate   (lua_State* L);
        static void     init        (void);
        bool            run         (GeoDataFrame* dataframe) override;

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            int Wind;
            char JerlovType[16];
            double a;
            double b;
            double c;
        } entry_t;

        typedef enum {
            SNR_DIM = 0,
            THU_DIM = 1,
            TRANSPORT_DIM = 2,
            NUM_DIMS = 3,
        } uncertainty_dim_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        Atl24Uncertainty  (lua_State* L, BathyParameters* _parms);
        ~Atl24Uncertainty (void) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const int            NUM_POINTING_ANGLES = 6;
        static const int            NUM_WIND_SPEEDS = 10;
        static const int            NUM_KDS = 50;

        static const int            WIND_SPEED_INDEX[NUM_WIND_SPEEDS];
        static const int            KD_INDEX[NUM_KDS];

        static const char*          UNCERTAINTY_FILENAMES[NUM_DIMS][NUM_POINTING_ANGLES];

        static const int            NUM_TABLE_ENTRIES = 25;
        static entry_t              SNR[NUM_POINTING_ANGLES][NUM_TABLE_ENTRIES];
        static entry_t              THU[NUM_POINTING_ANGLES][NUM_TABLE_ENTRIES];
        static entry_t              TRANSPORT[NUM_POINTING_ANGLES][NUM_TABLE_ENTRIES];

        BathyParameters*            parms;
};

#endif