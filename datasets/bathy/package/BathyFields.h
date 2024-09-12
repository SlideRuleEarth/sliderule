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
                          {"model_as_poisoon",    &modelAsPoisson} }) {};

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
                          {"ri_water",          &RIAir} }) {};

    ~RefractionFields(void) override = default;
};

/**********************/
/* Uncertainty Fields */
/**********************/
struct UncertaintyFields: public FieldDictionary 
{
    FieldElement<string>    assetKdName {"viirsj1-s3"}; // global water refractive index mask downloaded in atl24 init lua routine
    FieldElement<string>    resourceKd;                 // refraction index of air
    
    Asset* assetKd {NULL};

    UncertaintyFields(void): 
        FieldDictionary({ {"asset_kd",      &assetKdName},
                          {"resource_kd",   &resourceKd} }) {
        assetKd = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetKdName.value.c_str(), Asset::OBJECT_TYPE));
        if(!assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to find asset %s", assetKdName.value.c_str());
    };

    ~UncertaintyFields(void) override {
        if(assetKd) assetKd->releaseLuaObject();
    };
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

        static const char* phRecType;
        static const RecordObject::fieldDef_t phRecDef[];
        static const char* exRecType;
        static const RecordObject::fieldDef_t exRecDef[];

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
            SEA_SURFACE_UNDETECTED  = 0x02
        } flags_t;

        /* Photon Fields */
        typedef struct {
            int64_t         time_ns;                // nanoseconds since GPS epoch
            int32_t         index_ph;               // unique index of photon in granule
            int32_t         index_seg;              // index into segment level groups in source ATL03 granule
            double          lat_ph;                 // latitude of photon (EPSG 7912)
            double          lon_ph;                 // longitude of photon (EPSG 7912)
            double          x_ph;                   // the easting coordinate in meters of the photon for the given UTM zone
            double          y_ph;                   // the northing coordinate in meters of the photon for the given UTM zone
            double          x_atc;                  // along track distance calculated from segment_dist_x and dist_ph_along
            double          y_atc;                  // dist_ph_across
            double          background_rate;        // PE per second
            float           delta_h;                // refraction correction of height
            float           surface_h;              // orthometric height of sea surface at each photon location
            float           ortho_h;                // geoid corrected height of photon, calculated from h_ph and geoid
            float           ellipse_h;              // height of photon with respect to reference ellipsoid
            float           sigma_thu;              // total horizontal uncertainty
            float           sigma_tvu;              // total vertical uncertainty
            uint32_t        processing_flags;       // bit mask of flags for capturing errors and warnings
            uint8_t         yapc_score;             // atl03 density estimate (Yet Another Photon Classifier)
            int8_t          max_signal_conf;        // maximum value in the atl03 confidence table
            int8_t          quality_ph;             // atl03 quality flags
            int8_t          class_ph;               // photon classification
            int8_t          predictions[NUM_CLASSIFIERS];
        } photon_t;

        /* Extent Record */
        typedef struct {
            uint8_t         region;
            uint8_t         track;                  // 1, 2, or 3
            uint8_t         pair;                   // 0 (l), 1 (r)
            uint8_t         spot;                   // 1, 2, 3, 4, 5, 6
            uint16_t        reference_ground_track;
            uint8_t         cycle;
            uint8_t         utm_zone;
            uint64_t        extent_id;
            float           wind_v;                 // wind speed (in meters/second)            
            float           ndwi;                   // normalized difference water index using HLS data
            uint32_t        photon_count;
            photon_t        photons[];              // zero length field
        } extent_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);
        static void init        (void);

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<string>                            atl09AssetName;             // name of the asset in the asset directory for the ATL09 granules
        FieldElement<string>                            atl09Resource {"icesat2"};  // name of the ATL09 granule (including extension)
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

        Asset* asset09 {NULL}; // asset for ATL09 resources

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        BathyFields     (lua_State* L, int index);
        ~BathyFields    (void) override;
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

int convertToLua(lua_State* L, const BathyFields::classifier_t& v);
void convertFromLua(lua_State* L, int index, BathyFields::classifier_t& v);
int convertToIndex(const BathyFields::classifier_t& v);
void convertFromIndex(int index, BathyFields::classifier_t& v);

#endif  /* __bathy_fields__ */
