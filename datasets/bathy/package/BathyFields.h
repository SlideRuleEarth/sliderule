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
 * INCLUDES
 ******************************************************************************/

#define COASTNET_MODEL  "coastnet_model-20241111.json"
#define QTREES_MODEL    "qtrees_model-20241105.json"
#define ENSEMBLE_MODEL  "ensemble_model-20241115.json"
#define POINTNET_MODEL  "pointnet2_model.pth"

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

/*******************/
/* Coastnet Fields */
/*******************/
struct CoastnetFields: public FieldDictionary
{
    FieldElement<string>  model             {COASTNET_MODEL};
    FieldElement<bool>    setClass          {true};
    FieldElement<bool>    setSurface        {true};
    FieldElement<bool>    verbose           {true};

    CoastnetFields(void):
        FieldDictionary({ {"model",             &model},
                          {"set_class",         &setClass},
                          {"set_surface",       &setSurface},
                          {"verbose",           &verbose} }) {
    };

    virtual ~CoastnetFields(void) override = default;
};

/***********************/
/* OpenOceansPP Fields */
/***********************/
struct OpenOceansPPFields: public FieldDictionary
{
    FieldElement<bool>      setClass                {false};
    FieldElement<bool>      setSurface              {false};
    FieldElement<bool>      verbose                 {true};

    OpenOceansPPFields(void):
        FieldDictionary({ {"set_class",                 &setClass},
                          {"set_surface",               &setSurface},
                          {"verbose",                   &verbose} }) {};

    virtual ~OpenOceansPPFields(void) override = default;
};

/*****************/
/* Qtrees Fields */
/*****************/
struct QtreesFields: public FieldDictionary
{
    FieldElement<string>  model         {QTREES_MODEL};
    FieldElement<bool>    setClass      {false};
    FieldElement<bool>    setSurface    {false};
    FieldElement<bool>    verbose       {true};

    QtreesFields(void):
        FieldDictionary({ {"model",         &model},
                          {"set_class",     &setClass},
                          {"set_surface",   &setSurface},
                          {"verbose",       &verbose} }) {
    };

    virtual ~QtreesFields(void) override = default;
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

        inline static const char* QTREES_NAME           = "qtrees";
        inline static const char* COASTNET_NAME         = "coastnet";
        inline static const char* OPENOCEANSPP_NAME     = "openoceanspp";
        inline static const char* MEDIANFILTER_NAME     = "medianfilter";
        inline static const char* CSHELPH_NAME          = "cshelph";
        inline static const char* BATHYPATHFINDER_NAME  = "bathypathfinder";
        inline static const char* POINTNET_NAME         = "pointnet";
        inline static const char* OPENOCEANS_NAME       = "openoceans";
        inline static const char* ENSEMBLE_NAME         = "ensemble";

        static const double NIGHT_SOLAR_ELEVATION_THRESHOLD;
        static const double MINIMUM_HORIZONTAL_SUBAQUEOUS_UNCERTAINTY;
        static const double MINIMUM_VERTICAL_SUBAQUEOUS_UNCERTAINTY;
        static const double DEFAULT_WIND_SPEED;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Photon Classifiers */
        typedef enum {
            QTREES              = 0,
            COASTNET            = 1,
            OPENOCEANSPP        = 2,
            MEDIANFILTER        = 3,
            CSHELPH             = 4,
            BATHYPATHFINDER     = 5,
            POINTNET            = 6,
            OPENOCEANS          = 7,
            ENSEMBLE            = 8,
            NUM_CLASSIFIERS     = 9
        } classifier_t;

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
            BATHY_SIGNAL            = 0x00FF0000,
            BATHY_QTREES            = 0x01000000,
            BATHY_COASTNET          = 0x02000000,
            BATHY_OPENOCEANSPP      = 0x04000000,
            BATHY_MEDIANFILTER      = 0x08000000,
            BATHY_CSHELPH           = 0x10000000,
            BATHY_BATHYPATHFINDER   = 0x20000000,
            BATHY_POINTNET          = 0x40000000,
            BATHY_OPENOCEANS        = 0x80000000
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
        FieldElement<bool>                              findSeaSurface {false};     // locally implemented sea surface finder
        FieldEnumeration<classifier_t, NUM_CLASSIFIERS> classifiers {true, true, true, true, true, true, true, true, true}; // which bathymetry classifiers to run
        FieldEnumeration<spot_t, NUM_SPOTS>             spots = {true, true, true, true, true, true}; // only used by downstream algorithms
        SurfaceFields                                   surface;                    // surface finding fields
        RefractionFields                                refraction;                 // refraction correction fields
        UncertaintyFields                               uncertainty;                // uncertaintly calculation fields
        CoastnetFields                                  coastnet;                   // coastnet fields
        OpenOceansPPFields                              openoceanspp;               // openoceans++ fields
        QtreesFields                                    qtrees;                     // qtrees fields
        FieldElement<string>                            coastnetVersion {COASTNETINFO}; // git commit information for coastnet repo
        FieldElement<string>                            qtreesVersion {QTREESINFO}; // git commit information for qtrees repo
        FieldElement<string>                            openoceansppVersion {OPENOCEANSPPINFO}; // git commit information for openoceans repo

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                BathyFields     (lua_State* L, uint64_t key_space, const char* default_asset_name, const char* default_resource);
        virtual ~BathyFields    (void) override = default;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const BathyFields::classifier_t& v);
int convertToLua(lua_State* L, const BathyFields::classifier_t& v);
void convertFromLua(lua_State* L, int index, BathyFields::classifier_t& v);
int convertToIndex(const BathyFields::classifier_t& v);
void convertFromIndex(int index, BathyFields::classifier_t& v);

inline uint32_t toEncoding(BathyFields::classifier_t& v) { (void)v; return Field::INT32; }

#endif  /* __bathy_fields__ */
