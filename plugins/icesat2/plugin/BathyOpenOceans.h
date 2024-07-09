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
#include "BathyFields.h"

using BathyFields::extent_t;

/******************************************************************************
 * BATHY OPENOCEANS
 ******************************************************************************/

class BathyOpenOceans
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* OPENOCEANS_PARMS;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct parms_t {
            Asset*          assetKd;                // asset for reading Kd resources
            const char*     resourceKd;             // filename for Kd (uncertainty calculation)
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

                BathyOpenOceans         (lua_State* L, int index);
                ~BathyOpenOceans        (void) = default;
        void    findSeaSurface          (extent_t& extent) const;
        void    refractionCorrection    (extent_t& extent) const;

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        parms_t             parms;
        H5Coro::context_t   contextVIIRSJ1;
};

#endif