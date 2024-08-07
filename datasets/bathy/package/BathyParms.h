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

#ifndef __bathy_parms__
#define __bathy_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "GeoParms.h"
#include "BathyClassifier.h"
#include "BathyFields.h"
#include "Icesat2Parms.h"

/******************************************************************************
 * REQUEST PARAMETERS
 ******************************************************************************/

class BathyParms: public Icesat2Parms
{
    public:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Bathymetry Reader */
        struct reader_t {
            Asset*                      asset;              // asset for ATL03 resources
            Asset*                      asset09;            // asset for ATL09 resources
            GeoParms*                   hls;                // geo-package parms for sampling HLS for NDWI
            const char*                 resource09;         // ATL09 granule
            double                      max_dem_delta;      // initial filter of heights against DEM (For removing things like clouds)
            double                      min_dem_delta;      // initial filter of heights against DEM (For removing things like clouds)
            int                         ph_in_extent;       // number of photons in each extent
            bool                        generate_ndwi;      // use HLS data to generate NDWI for each segment lat,lon
            bool                        use_bathy_mask;     // global bathymetry mask downloaded in atl24 init lua routine
            bool                        classifiers[BathyFields::NUM_CLASSIFIERS]; // which bathymetry classifiers to run
            bool                        return_inputs;      // return the atl03 bathy records back to client
            bool                        spots[NUM_SPOTS];   // only used by downstream algorithms
            bool                        output_as_sdp;      // include all the necessary ancillary data for the standard data product
            double                      bin_size;           // meters
            double                      max_range;          // meters
            long                        max_bins;           // bins
            double                      signal_threshold;   // standard deviations
            double                      min_peak_separation;// meters
            double                      highest_peak_ratio;
            double                      surface_width;      // standard deviations
            bool                        model_as_poisson;
            
            reader_t():
                asset                   (NULL),
                asset09                 (NULL), 
                hls                     (NULL),
                resource09              (NULL),
                max_dem_delta           (50.0),
                min_dem_delta           (-100.0),
                ph_in_extent            (8192),
                generate_ndwi           (false),
                use_bathy_mask          (true),
                classifiers             {true, true, true, true, true, true, true, true, true},
                return_inputs           (false),
                spots                   {true, true, true, true, true, true},
                output_as_sdp           (false),
                bin_size                (0.5),
                max_range               (1000.0),
                max_bins                (10000),
                signal_threshold        (3.0),
                min_peak_separation     (0.5),
                highest_peak_ratio      (1.2),
                surface_width           (3.0),
                model_as_poisson        (true) {};
            ~reader_t() {
                if(asset) asset->releaseLuaObject();
                if(asset09) asset09->releaseLuaObject();
                if(hls) hls->releaseLuaObject();
                delete [] resource09;
            };

            void        fromLua (lua_State* L, int index);
            const char* toJson  (void) const;
        };

        /* Refraction Correction */
        struct refraction_t {
            double          ri_air;                 // refraction index of air
            double          ri_water;               // refraction index of water

            refraction_t():
                ri_air      (1.00029),
                ri_water    (1.34116) {};

            ~refraction_t() = default;

            void        fromLua (lua_State* L, int index);
            const char* toJson  (void) const;
        };

        /* Uncertainty Calculation */
        struct uncertainty_t {
            Asset*          assetKd;                // asset for reading Kd resources
            const char*     resourceKd;             // filename for Kd (uncertainty calculation)

            uncertainty_t():
                assetKd             (NULL),
                resourceKd          (NULL) {};

            ~uncertainty_t() {
                if(assetKd) assetKd->releaseLuaObject();
                delete [] resourceKd;
            };

            void        fromLua (lua_State* L, int index);
            const char* toJson  (void) const;
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        const char* tojson      (void) const override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        reader_t        reader;
        refraction_t    refraction;
        uncertainty_t   uncertainty;

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        BathyParms      (lua_State* L, int index);
                        ~BathyParms     (void) override;
        static void     getSpotList     (lua_State* L, int index, bool* provided, bool* spots, int size=Icesat2Parms::NUM_SPOTS);
        static void     getClassifiers  (lua_State* L, int index, bool* provided, bool* classifiers, int size=BathyFields::NUM_CLASSIFIERS);
};

#endif  /* __bathy_parms__ */
