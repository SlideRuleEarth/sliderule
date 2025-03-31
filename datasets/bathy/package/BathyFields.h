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

#ifndef __bathy_fields__
#define __bathy_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "Asset.h"
#include "Icesat2Fields.h"
#include "RecordObject.h"

/******************************************************************************
 * CLASSES
 ******************************************************************************/

/******************/
/* Surface Fields */
/******************/
struct SurfaceFields: public FieldDictionary
{
    FieldElement<double>    binSize             {0.5};      // meters
    FieldElement<double>    maxRange            {1000.0};   // meters
    FieldElement<long>      maxBins             {10000};    // bins
    FieldElement<double>    signalThreshold     {3.0};      // standard deviations
    FieldElement<double>    minPeakSeparation   {0.5};      // meters
    FieldElement<double>    highestPeakRatio    {1.2};
    FieldElement<double>    surfaceWidth        {3.0};      // standard deviations
    FieldElement<bool>      modelAsPoisson      {true};

    SurfaceFields(void):
        FieldDictionary({ {"bin_size",            &binSize},
                          {"max_range",           &maxRange},
                          {"max_bins",            &maxBins},
                          {"signal_threshold",    &signalThreshold},
                          {"min_peak_separation", &minPeakSeparation},
                          {"highest_peak_ration", &highestPeakRatio},
                          {"surace_width",        &surfaceWidth},
                          {"model_as_poisoon",    &modelAsPoisson} }) {
    };

    virtual ~SurfaceFields(void) override = default;
};

/*********************/
/* Refraction Fields */
/*********************/
struct RefractionFields: public FieldDictionary
{
    FieldElement<bool>      useWaterRIMask  {true};     // global water refractive index mask downloaded in atl24 init lua routine
    FieldElement<double>    RIAir           {1.00029};  // refraction index of air
    FieldElement<double>    RIWater         {1.34116};  // refraction index of water

    RefractionFields(void):
        FieldDictionary({ {"use_water_ri_mask", &useWaterRIMask},
                          {"ri_air",            &RIAir},
                          {"ri_water",          &RIAir} }) {
    };

    virtual ~RefractionFields(void) override = default;
};

/**********************/
/* Uncertainty Fields */
/**********************/
struct UncertaintyFields: public FieldDictionary
{
    AssetField assetKd {"viirsj1-s3"}; // global water refractive index mask downloaded in atl24 init lua routine

    UncertaintyFields(void):
        FieldDictionary({ {"asset_kd", &assetKd} }) {
    };

    virtual ~UncertaintyFields(void) override = default;
};

/****************/
/* Bathy Fields */
/****************/
class BathyFields: public Icesat2Fields
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const double NIGHT_SOLAR_ELEVATION_THRESHOLD;
        static const double MINIMUM_HORIZONTAL_SUBAQUEOUS_UNCERTAINTY;
        static const double MINIMUM_VERTICAL_SUBAQUEOUS_UNCERTAINTY;
        static const double DEFAULT_WIND_SPEED;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Photon Classifications */
        typedef enum {
            UNCLASSIFIED        = 0,
            OTHER               = 1,
            BATHYMETRY          = 40,
            SEA_SURFACE         = 41,
            WATER_COLUMN        = 45
        } bathy_class_t;

        /* Processing Flags */
        typedef enum {
            FLAGS_CLEAR             = 0x00000000,
            ON_BOUNDARY             = 0x00000001, // set if photon is first after a spatial boundary
            SENSOR_DEPTH_EXCEEDED   = 0x00000002,
            SEA_SURFACE_UNDETECTED  = 0x00000004,
            INVALID_KD              = 0x00000008,
            INVALID_WIND_SPEED      = 0x00000010,
            NIGHT_FLAG              = 0x00000020,
            BATHY_CONFIDENCE        = 0x0000FF00,
            YAPC_SCORE              = 0x00FF0000,
            BATHY_SIGNAL            = 0xFF000000,
        } flags_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate       (lua_State* L);
        static int  luaClassifier   (lua_State* L);
        void        fromLua         (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<string>                            atl09AssetName {"icesat2"}; // name of the asset in the asset directory for the ATL09 granules
        FieldElement<double>                            maxDemDelta {50.0};         // initial filter of heights against DEM (For removing things like clouds)
        FieldElement<double>                            minDemDelta {-100.0};       // initial filter of heights against DEM (For removing things like clouds)
        FieldElement<double>                            maxGeoidDelta {50.0};       // initial filter of heights against geoid (For removing things like clouds)
        FieldElement<double>                            minGeoidDelta {-100.0};     // initial filter of heights against geoid (For removing things like clouds)
        FieldElement<int>                               phInExtent {8192};          // number of photons in each extent
        FieldElement<bool>                              generateNdwi {false};       // use HLS data to generate NDWI for each segment lat,lon
        FieldElement<bool>                              useBathyMask {true};        // global bathymetry mask downloaded in atl24 init lua routine
        FieldEnumeration<spot_t, NUM_SPOTS>             spots = {true, true, true, true, true, true}; // only used by downstream algorithms
        SurfaceFields                                   surface;                    // surface finding fields
        RefractionFields                                refraction;                 // refraction correction fields
        UncertaintyFields                               uncertainty;                // uncertaintly calculation fields

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                BathyFields     (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource);
        virtual ~BathyFields    (void) override = default;
};

#endif  /* __bathy_fields__ */
