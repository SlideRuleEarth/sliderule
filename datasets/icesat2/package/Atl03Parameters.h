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

#ifndef __atl03_fields__
#define __atl03_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "Icesat2Parameters.h"
#include "GeoDataFrame.h"
#include "FieldElement.h"
#include "FieldEnumeration.h"
#include "FieldMap.h"
#include "FieldList.h"
#include "TimeLib.h"

/******************************************************************************
 * CLASSES
 ******************************************************************************/

/**************/
/* Fit Fields */
/**************/
struct FitFields: public FieldMap<Field>
{

    FieldElement<int>       maxIterations {5};          // least squares fit iterations
    FieldElement<double>    minWindow {3.0};            // H_win minimum
    FieldElement<double>    maxRobustDispersion {5.0};  // sigma_r

    FitFields(void);
    ~FitFields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/***************/
/* YAPC Fields */
/***************/
struct YapcFields: public FieldMap<Field>
{
    FieldElement<uint16_t>  score {0};      // minimum allowed weight of photon using yapc algorithm
    FieldElement<int>       version {0};    // version of the yapc algorithm to run
    FieldElement<int>       knn {0};        // (version 2 only) k-nearest neighbors
    FieldElement<int>       min_knn {5};    // (version 3 only) minimum number of k-nearest neighbors
    FieldElement<double>    win_h {6.0};    // window height (overrides calculated value if non-zero)
    FieldElement<double>    win_x {15.0};   // window width

    YapcFields(void);
    ~YapcFields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/******************/
/* PhoREAL Fields */
/******************/
struct PhorealFields: public FieldMap<Field>
{
    typedef enum {
        MEAN = 0,
        MEDIAN = 1,
        CENTER = 2
    } phoreal_geoloc_t;

    FieldElement<double>            binsize {1.0};              // size of photon height bin
    FieldElement<phoreal_geoloc_t>  geoloc {MEDIAN};            // how are geolocation stats calculated
    FieldElement<bool>              use_abs_h {false};          // use absolute heights
    FieldElement<bool>              send_waveform {false};      // include the waveform in the results
    FieldElement<bool>              above_classifier {false};   // use the ABoVE classification algorithm
    FieldElement<int8_t>            te_quality_filter {0};      // minimum allowed terrain quality score
    FieldElement<int8_t>            can_quality_filter {0};     // minimum allowed canopy quality score

    PhorealFields(void);
    ~PhorealFields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool te_quality_filter_provided;
    bool can_quality_filter_provided;
    bool provided;
};

/******************/
/* Blanket Fields */
/******************/
struct BlanketFields: public FieldMap<Field>
{
    FieldElement<double>    max_top_of_surface_percentile {0.98};
    FieldElement<double>    median_ground_percentile {0.50};

    BlanketFields(void);
    ~BlanketFields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/****************/
/* Atl24 Fields */
/****************/
struct Atl24Fields: public FieldMap<Field>
{
    typedef enum {
        UNCLASSIFIED        = 0,
        BATHYMETRY          = 40,
        SEA_SURFACE         = 41,
        NUM_CLASSES         = 3
    } class_t;

    typedef enum {
        FLAG_OFF = 0,
        FLAG_ON = 1,
        NUM_FLAGS = 2
    } flag_t;

    FieldElement<bool>                      compact {true};                     // reduce number of fields from atl24
    FieldEnumeration<class_t,NUM_CLASSES>   class_ph {false, true, false};      // list of desired bathymetry classes of photons
    FieldElement<double>                    confidence_threshold {0.0};         // filter based on confidence
    FieldEnumeration<flag_t,NUM_FLAGS>      invalid_kd {true, true};            // filter on invalid kd flag
    FieldEnumeration<flag_t,NUM_FLAGS>      invalid_wind_speed {true, true};    // filter on invalid wind speed
    FieldEnumeration<flag_t,NUM_FLAGS>      low_confidence {true, true};        // filter on low confidence flag
    FieldEnumeration<flag_t,NUM_FLAGS>      night {true, true};                 // filter based on night flag
    FieldEnumeration<flag_t,NUM_FLAGS>      sensor_depth_exceeded {true, true}; // filter based on sensor depth exceeded flag
    FieldList<string>                       anc_fields;                         // list of additional ATL24 fields

    Atl24Fields(void);
    ~Atl24Fields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/****************/
/* ATL03 Fields */
/****************/
class Atl03Parameters: public Icesat2Parameters
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        virtual void    fromLua             (lua_State* L, int index) override;
                        Atl03Parameters     (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name = LUA_META_NAME);
        virtual         ~Atl03Parameters    (void) override = default;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<surface_type_t>                        surfaceType {SRT_DYNAMIC};                              // surface reference type (used to select signal confidence column)
        FieldElement<bool>                                  passInvalid {false};                                    // post extent even if each pair is invalid
        FieldElement<bool>                                  distInSeg {false};                                      // the extent length and step are expressed in segments, not meters
        FieldEnumeration<signal_conf_t,NUM_SIGNAL_CONF>     atl03Cnf {false, false, false, false, true, true, true}; // list of desired signal confidences of photons from atl03 classification
        FieldEnumeration<quality_ph_t,NUM_PHOTON_QUALITY>   qualityPh { true,  false, false, false, false, false,   // list of desired photon quality levels from atl03
                                                                        false, false, false, false, false, false,
                                                                        false, false, false, false, false, false,
                                                                        false, false, false, false, false, false,
                                                                        false, false };
        FieldEnumeration<atl08_class_t,NUM_ATL08_CLASSES>   atl08Class {false, false, false, false, false};         // list of surface classifications to use (leave empty to skip)
        FieldElement<int>                                   minPhotonCount {10};                                    // PE
        FieldElement<double>                                minAlongTrackSpread {20.0};                             // meters
        FieldElement<double>                                extentLength {40.0};                                    // length of ATL06 extent (meters or segments if dist_in_seg is true)
        FieldElement<double>                                extentStep {20.0};                                      // resolution of the ATL06 extent (meters or segments if dist_in_seg is true)
        FieldElement<uint8_t>                               podppdMask {0x01};                                      // 0: nominal, 1: pod_degrade, 2: ppd_degrade, 3: podppd_degrade, 4: cal_nominal, 5: cal_pod_degrade, 6: cal_ppd_degrade, 7: cal_podppd_degrade
        FitFields                                           fit;                                                    // settings used in the surface fitter algorithm
        YapcFields                                          yapc;                                                   // settings used in YAPC algorithm
        PhorealFields                                       phoreal;                                                // phoreal algorithm settings
        BlanketFields                                       blanket;                                                // blanket algorithm settings
        Atl24Fields                                         atl24;                                                  // atl24 algorithm settings
        FieldList<string>                                   atl03BckgrdFields;                                      // list of background fields to associate with an extent
        FieldList<string>                                   atl03GeoFields;                                         // list of geolocation fields to associate with an extent
        FieldList<string>                                   atl03CorrFields;                                        // list of geophys_corr fields to associate with an extent
        FieldList<string>                                   atl03PhFields;                                          // list of per-photon fields to associate with an extent
        FieldList<string>                                   atl08Fields;                                            // list of ATL08 fields to associate with an extent
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const PhorealFields::phoreal_geoloc_t& v);
int convertToLua(lua_State* L, const PhorealFields::phoreal_geoloc_t& v);
void convertFromLua(lua_State* L, int index, PhorealFields::phoreal_geoloc_t& v);

string convertToJson(const Atl24Fields::class_t& v);
int convertToLua(lua_State* L, const Atl24Fields::class_t& v);
void convertFromLua(lua_State* L, int index, Atl24Fields::class_t& v);
int convertToIndex(const Atl24Fields::class_t& v);
void convertFromIndex(int index, Atl24Fields::class_t& v);

string convertToJson(const Atl24Fields::flag_t& v);
int convertToLua(lua_State* L, const Atl24Fields::flag_t& v);
void convertFromLua(lua_State* L, int index, Atl24Fields::flag_t& v);
int convertToIndex(const Atl24Fields::flag_t& v);
void convertFromIndex(int index, Atl24Fields::flag_t& v);

inline uint32_t toEncoding(PhorealFields::phoreal_geoloc_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Atl24Fields::class_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Atl24Fields::flag_t& v) { (void)v; return Field::INT32; }

#endif  /* __atl03_fields__ */
