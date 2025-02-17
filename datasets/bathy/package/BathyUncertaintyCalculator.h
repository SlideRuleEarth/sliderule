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

#ifndef __bathy_uncertainty_calculator__
#define __bathy_uncertainty_calculator__

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "H5Coro.h"
#include "H5Array.h"
#include "BathyFields.h"
#include "BathyDataFrame.h"
#include "BathyKd.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class BathyUncertaintyCalculator: public GeoDataFrame::FrameRunner
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
        static int      luaInit     (lua_State* L);


        bool            run         (GeoDataFrame* dataframe) override;

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            int Wind;
            double Kd;
            double b;
            double c;
        } uncertainty_entry_t;

        typedef struct {
            double b;
            double c;
        } uncertainty_coeff_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        BathyUncertaintyCalculator  (lua_State* L, BathyFields* _parms, BathyKd* _kd);
        ~BathyUncertaintyCalculator (void) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const int            NUM_POINTING_ANGLES = 6;
        static const int            NUM_WIND_SPEED_RANGES = 5;
        static const int            NUM_KD_RANGES = 5;
        static const int            NUM_UNCERTAINTY_DIMENSIONS = 2;
        static const int            THU = 0;
        static const int            TVU = 1;
        static const int            INITIAL_UNCERTAINTY_ROWS = 310;

        static const char*          TU_FILENAMES[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES];
        static const int            POINTING_ANGLES[NUM_POINTING_ANGLES];
        static const int            WIND_SPEED_RANGES[NUM_WIND_SPEED_RANGES][2];
        static const double         KD_RANGES[NUM_KD_RANGES][2];

        static uncertainty_coeff_t  UNCERTAINTY_COEFF_MAP[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES][NUM_WIND_SPEED_RANGES][NUM_KD_RANGES];

        BathyFields*                parms;
        BathyKd*                    kd490;
};

#endif