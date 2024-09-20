/*
 * Copyright (c) 2021, University of Washington
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

#ifndef __bathy_openoceanspp_classifier__
#define __bathy_openoceanspp_classifier__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "LuaObject.h"
#include "OsApi.h"
#include "GeoDataFrame.h"
#include "BathyFields.h"

/******************************************************************************
 * BATHY CLASSIFIER
 ******************************************************************************/

class BathyOpenOceansPPClassifier: public GeoDataFrame::FrameRunner
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* CLASSIFIER_NAME;
        static const char* OPENOCEANSPP_PARMS;
        static const char* DEFAULT_OPENOCEANSPP_MODEL;

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        struct parms_t {
            bool            set_class;          // whether to update class_ph in extent
            bool            set_surface;        // whether to update surface_h in extent
            bool            use_predictions;    // whether to update surface_h in extent
            bool            verbose;            // verbose settin gin XGBoost library
            double          x_resolution;
            double          z_resolution;
            double          z_min;
            double          z_max;
            double          surface_z_min;
            double          surface_z_max;
            double          bathy_min_depth;
            double          vertical_smoothing_sigma;
            double          surface_smoothing_sigma;
            double          bathy_smoothing_sigma;
            double          min_peak_prominence;
            size_t          min_peak_distance;
            size_t          min_surface_photons_per_window;
            size_t          min_bathy_photons_per_window;

            parms_t(): 
                set_class (false),
                set_surface (false),
                use_predictions (false),
                verbose (true),
                x_resolution (10.0),
                z_resolution (0.2),
                z_min (-50),
                z_max (30),
                surface_z_min (-20),
                surface_z_max (20),
                bathy_min_depth (0.5),
                vertical_smoothing_sigma (0.5),
                surface_smoothing_sigma (200.0),
                bathy_smoothing_sigma (100.0),
                min_peak_prominence (0.01),
                min_peak_distance (2),
                min_surface_photons_per_window ((x_resolution / 0.7) / 3),
                min_bathy_photons_per_window ((x_resolution / 0.7) / 3) {};
            ~parms_t() {};
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

        bool run (GeoDataFrame* dataframe) override;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        BathyOpenOceansPPClassifier (lua_State* L, int index);
        ~BathyOpenOceansPPClassifier (void) override = default;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        parms_t parms;
};

#endif  /* __bathy_openoceanspp_classifier__ */
