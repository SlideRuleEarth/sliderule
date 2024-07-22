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

#ifndef __bathy_openoceans__
#define __bathy_openoceans__

#include "OsApi.h"
#include "H5Coro.h"
#include "H5Array.h"
#include "NetsvcParms.h"
#include "BathyFields.h"

using BathyFields::extent_t;

/******************************************************************************
 * BATHY OPENOCEANS
 ******************************************************************************/

class BathyOceanEyes
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OCEANEYES_PARMS;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct parms_t {
            Asset*          assetKd;                // asset for reading Kd resources
            const char*     resourceKd;             // filename for Kd (uncertainty calculation)
            long            read_timeout_ms;        // timeout for reading Kd
            double          ri_air;
            double          ri_water;
            double          dem_buffer;             // meters
            double          bin_size;               // meters
            double          max_range;              // meters
            long            max_bins;               // bins
            double          signal_threshold;       // standard deviations
            double          min_peak_separation;    // meters
            double          highest_peak_ratio;
            double          surface_width;          // standard deviations
            bool            model_as_poisson;

            parms_t():
                assetKd             (NULL),
                resourceKd          (NULL),
                read_timeout_ms     (NetsvcParms::DEFAULT_READ_TIMEOUT * 1000), // TODO: this is where we need to inherit the value from the request level parameters
                ri_air              (1.00029),
                ri_water            (1.34116),
                dem_buffer          (50.0),
                bin_size            (0.5),
                max_range           (1000.0),
                max_bins            (10000),
                signal_threshold    (3.0),
                min_peak_separation (0.5),
                highest_peak_ratio  (1.2),
                surface_width       (3.0),
                model_as_poisson    (true) {};

            ~parms_t() {
                if(assetKd) assetKd->releaseLuaObject();
                delete [] resourceKd;
            };
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void     init                    (void);
                        BathyOceanEyes          (lua_State* L, int index);
                        ~BathyOceanEyes         (void);
        void            findSeaSurface          (extent_t& extent) const;
        void            correctRefraction       (extent_t& extent) const;
        void            calculateUncertainty    (extent_t& extent) const;

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            int Wind;
            double Kd;
            double a;
            double b;
            double c;
        } uncertainty_entry_t;

        typedef struct {
            double a;
            double b;
            double c;
        } uncertainty_coeff_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static const int            NUM_POINTING_ANGLES = 6;
        static const int            NUM_WIND_SPEEDS = 10;
        static const int            NUM_KD_RANGES = 5;
        static const int            NUM_UNCERTAINTY_DIMENSIONS = 2;
        static const int            THU = 0;
        static const int            TVU = 1;
        static const int            INITIAL_UNCERTAINTY_ROWS = 310;

        static const char*          TU_FILENAMES[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES];
        static const int            POINTING_ANGLES[NUM_POINTING_ANGLES];
        static const int            WIND_SPEEDS[NUM_WIND_SPEEDS];
        static const double         KD_RANGES[NUM_KD_RANGES][2];

        static uncertainty_coeff_t  UNCERTAINTY_COEFF_MAP[NUM_UNCERTAINTY_DIMENSIONS][NUM_POINTING_ANGLES][NUM_WIND_SPEEDS][NUM_KD_RANGES];

        parms_t                     parms;
        H5Coro::Context*            context;
        H5Array<int16_t>*           Kd_490;
};

#endif