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
        initialized = true;
    };

    ~SurfaceFields(void) override = default;
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
        initialized = true;
    };

    ~RefractionFields(void) override = default;
};

/**********************/
/* Uncertainty Fields */
/**********************/
struct UncertaintyFields: public FieldDictionary 
{
    FieldElement<string>    assetKdName {"viirsj1-s3"}; // global water refractive index mask downloaded in atl24 init lua routine
    
    Asset* assetKd {NULL};

    UncertaintyFields(void): 
        FieldDictionary({ {"asset_kd",      &assetKdName} }) {
        assetKd = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetKdName.value.c_str(), Asset::OBJECT_TYPE));
        if(!assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to find asset %s", assetKdName.value.c_str());
        initialized = true;
    };

    ~UncertaintyFields(void) override {
        if(assetKd) assetKd->releaseLuaObject();
    };
};

/*******************/
/* Coastnet Fields */
/*******************/
struct CoastnetFields: public FieldDictionary 
{
    FieldElement<string>  model             {"/data/coastnet_model-20240628.json"};
    FieldElement<bool>    setClass          {true};
    FieldElement<bool>    usePredictions    {false};
    FieldElement<bool>    verbose           {true};
    
    CoastnetFields(void): 
        FieldDictionary({ {"model",             &model},
                          {"set_class",         &setClass}, 
                          {"use_predictions",   &usePredictions}, 
                          {"verbose",           &verbose} }) {
        initialized = true;
    };

    ~CoastnetFields(void) override = default;
};

/***********************/
/* OpenOceansPP Fields */
/***********************/
struct OpenOceansPPFields: public FieldDictionary 
{
    FieldElement<bool>      setClass                   {false};
    FieldElement<bool>      setSurface                 {false};
    FieldElement<bool>      usePredictions             {false};
    FieldElement<bool>      verbose                     {true};
    FieldElement<double>    xResolution                {10.0};
    FieldElement<double>    zResolution                {0.2};
    FieldElement<double>    zMin                       {-50};
    FieldElement<double>    zMax                       {30};
    FieldElement<double>    surfaceZMin               {-20};
    FieldElement<double>    surfaceZMax               {20};
    FieldElement<double>    bathyMinDepth             {0.5};
    FieldElement<double>    verticalSmoothingSigma    {0.5};
    FieldElement<double>    surfaceSmoothingSigma     {200.0};
    FieldElement<double>    bathySmoothingSigma       {100.0};
    FieldElement<double>    minPeakProminence         {0.01};
    FieldElement<size_t>    minPeakDistance           {2};
    
    size_t                  minSurfacePhotonsPerWindow;
    size_t                  minBathyPhotonsPerWindow;
    
    OpenOceansPPFields(void): 
        FieldDictionary({ {"set_class",                 &setClass},
                          {"set_surface",               &setSurface}, 
                          {"use_predictions",           &usePredictions}, 
                          {"verbose",                   &verbose}, 
                          {"x_resolution",              &xResolution}, 
                          {"z_resolution",              &zResolution}, 
                          {"z_min",                     &zMin}, 
                          {"z_max",                     &zMax}, 
                          {"surface_z_min",             &surfaceZMin}, 
                          {"surface_z_max",             &surfaceZMax}, 
                          {"bathy_min_depth",           &bathyMinDepth}, 
                          {"vertical_smoothing_sigma",  &verticalSmoothingSigma}, 
                          {"surface_smoothing_sigma",   &surfaceSmoothingSigma}, 
                          {"bathy_smoothing_sigma",     &bathySmoothingSigma}, 
                          {"min_peak_prominence",       &minPeakProminence}, 
                          {"min_peak_distance",         &minPeakDistance} }) {
        minSurfacePhotonsPerWindow = (xResolution.value / 0.7) / 3;
        minBathyPhotonsPerWindow = (xResolution.value / 0.7) / 3;
        initialized = true;
    };

    ~OpenOceansPPFields(void) override = default;
};

/*****************/
/* Qtrees Fields */
/*****************/
struct QtreesFields: public FieldDictionary 
{
    FieldElement<string>  model         {"/data/qtrees_model-20240607.json"};
    FieldElement<bool>    setClass      {false};
    FieldElement<bool>    setSurface    {true};
    FieldElement<bool>    verbose       {true};
    
    QtreesFields(void): 
        FieldDictionary({ {"model",         &model},
                          {"set_class",     &setClass}, 
                          {"set_surface",   &setSurface}, 
                          {"verbose",       &verbose} }) {
        initialized = true;
    };

    ~QtreesFields(void) override = default;
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
            SENSOR_DEPTH_EXCEEDED   = 0x01,
            SEA_SURFACE_UNDETECTED  = 0x02,
            ON_BOUNDARY             = 0x03  // set if photon is first after a spatial boundary
        } flags_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<string>                            atl09AssetName {"icesat2"}; // name of the asset in the asset directory for the ATL09 granules
        FieldElement<double>                            maxDemDelta {50.0};         // initial filter of heights against DEM (For removing things like clouds)
        FieldElement<double>                            minDemDelta {-100.0};       // initial filter of heights against DEM (For removing things like clouds)
        FieldElement<int>                               phInExtent {8192};          // number of photons in each extent
        FieldElement<bool>                              generateNdwi {false};       // use HLS data to generate NDWI for each segment lat,lon
        FieldElement<bool>                              useBathyMask {true};        // global bathymetry mask downloaded in atl24 init lua routine
        FieldElement<bool>                              findSeaSurface {false};     // locally implemented sea surface finder
        FieldEnumeration<classifier_t, NUM_CLASSIFIERS> classifiers {true, true, true, true, true, true, true, true, true}; // which bathymetry classifiers to run
        FieldEnumeration<spot_t, NUM_SPOTS>             spots = {true, true, true, true, true, true}; // only used by downstream algorithms
        FieldElement<SurfaceFields>                     surface;                    // surface finding fields
        FieldElement<RefractionFields>                  refraction;                 // refraction correction fields
        FieldElement<UncertaintyFields>                 uncertainty;                // uncertaintly calculation fields
        FieldElement<CoastnetFields>                    coastnet;                   // coastnet fields
        FieldElement<OpenOceansPPFields>                openoceanspp;               // openoceans++ fields
        FieldElement<QtreesFields>                      qtrees;                     // qtrees fields

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        BathyFields     (lua_State* L, int index);
        ~BathyFields    (void) override = default;
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
inline uint32_t toEncoding(SurfaceFields& v) { (void)v; return Field::USER; }
inline uint32_t toEncoding(UncertaintyFields& v) { (void)v; return Field::USER; }
inline uint32_t toEncoding(RefractionFields& v) { (void)v; return Field::USER; }
inline uint32_t toEncoding(CoastnetFields& v) { (void)v; return Field::USER; }
inline uint32_t toEncoding(OpenOceansPPFields& v) { (void)v; return Field::USER; }
inline uint32_t toEncoding(QtreesFields& v) { (void)v; return Field::USER; }

#endif  /* __bathy_fields__ */
