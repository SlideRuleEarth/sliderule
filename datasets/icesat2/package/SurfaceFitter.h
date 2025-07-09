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

#ifndef __surface_fitter__
#define __surface_fitter__

#include "OsApi.h"
#include "GeoDataFrame.h"
#include "Icesat2Fields.h"
#include "Atl03DataFrame.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class SurfaceFitter: public GeoDataFrame::FrameRunner
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
        bool            run         (GeoDataFrame* dataframe) override;

    private:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

         static const double SPEED_OF_LIGHT;
         static const double PULSE_REPITITION_FREQUENCY;
         static const double RDE_SCALE_FACTOR;
         static const double SIGMA_BEAM;
         static const double SIGMA_XMIT;

         /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            uint32_t    p;                  // index into photon array
            double      r;                  // residual
            double      x;                  // x-axis (x_atc relative to extent)
        } point_t;

        struct result_t {
            int32_t     n_fit_photons = 0;  // number of photons in final fit
            uint16_t    pflags = 0;         // processing flags
            time8_t     time_ns {0};        // nanoseconds from GPS epoch
            double      latitude = 0;
            double      longitude = 0;
            double      h_mean = 0;         // meters from ellipsoid
            double      dh_fit_dx = 0;      // along track slope
            double      x_atc = 0;          // distance from equator
            double      y_atc = 0;          // distance from reference track
            double      h_sigma = 0;
            double      rms_misfit = 0;
            double      window_height = 0;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        SurfaceFitter  (lua_State* L, Icesat2Fields* _parms);
        ~SurfaceFitter (void) override;

        result_t        iterativeFitStage       (const Atl03DataFrame& df, int32_t start_photon, int32_t num_photons);
        static void     leastSquaresFit         (const Atl03DataFrame& df, point_t* array, int32_t size, bool final, result_t& result);
        static void     quicksort               (point_t* array, int32_t start, int32_t end);
        static int      quicksortpartition      (point_t* array, int32_t start, int32_t end);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        Icesat2Fields*  parms;
};

#endif