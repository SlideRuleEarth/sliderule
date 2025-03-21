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

#ifndef __icesat2_fields__
#define __icesat2_fields__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "LuaObject.h"
#include "EndpointProxy.h"
#include "AssetField.h"
#include "MathLib.h"
#include "RequestFields.h"
#include "FieldElement.h"
#include "FieldEnumeration.h"
#include "FieldDictionary.h"
#include "FieldList.h"
#include "TimeLib.h"
#include "Asset.h"

/******************************************************************************
 * CLASSES
 ******************************************************************************/

/**************/
/* Fit Fields */
/**************/
struct FitFields: public FieldDictionary
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
struct YapcFields: public FieldDictionary
{
    FieldElement<uint8_t>   score {0};      // minimum allowed weight of photon using yapc algorithm
    FieldElement<int>       version {3};    // version of the yapc algorithm to run
    FieldElement<int>       knn {0};        // (version 2 only) k-nearest neighbors
    FieldElement<int>       min_knn {5};    // (version 3 only) minimum number of k-nearest neighors
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
struct PhorealFields: public FieldDictionary
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

    PhorealFields(void);
    ~PhorealFields(void) override = default;

    virtual void fromLua (lua_State* L, int index) override;

    bool provided;
};

/******************/
/* Atl24 Fields */
/******************/
struct Atl24Fields: public FieldDictionary
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

/*******************/
/* ICESat-2 Fields */
/*******************/
class Icesat2Fields: public RequestFields
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int NUM_SPOTS                          = 6;

        static const int EXTENT_ID_PHOTONS                  = 0x0;
        static const int EXTENT_ID_ELEVATION                = 0x2;
        static const int EXPECTED_NUM_FIELDS                = 8; // a typical number of ancillary fields requested

        static const uint8_t INVALID_FLAG                   = 0xFF;

        static const int64_t ATLAS_SDP_EPOCH_GPS            = 1198800018; // seconds to add to ATLAS delta times to get GPS times

        static const uint32_t PFLAG_SPREAD_TOO_SHORT        = 0x0001; // ats
        static const uint32_t PFLAG_TOO_FEW_PHOTONS         = 0x0002; // cnt
        static const uint32_t PFLAG_MAX_ITERATIONS_REACHED  = 0x0004; // maxi
        static const uint32_t PFLAG_OUT_OF_BOUNDS           = 0x0008;
        static const uint32_t PFLAG_BIN_UNDERFLOW           = 0x0010;
        static const uint32_t PFLAG_BIN_OVERFLOW            = 0x0020;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        /* Single Tracks */
        typedef enum {
            RPT_L = 0,
            RPT_R = 1,
            NUM_PAIR_TRACKS = 2
        } rpt_t;

        /* Pair Tracks */
        typedef enum {
            ALL_TRACKS = 0,
            RPT_1 = 1,
            RPT_2 = 2,
            RPT_3 = 3,
            NUM_TRACKS = 3
        } track_t;

        /* Ground Tracks */
        typedef enum {
            INVALID_GT = 0,
            GT1L = 10,
            GT1R = 20,
            GT2L = 30,
            GT2R = 40,
            GT3L = 50,
            GT3R = 60
        } gt_t;

        /* Spots */
        typedef enum {
            INVALID_SPOT = 0,
            SPOT_1 = 1,
            SPOT_2 = 2,
            SPOT_3 = 3,
            SPOT_4 = 4,
            SPOT_5 = 5,
            SPOT_6 = 6
        } spot_t;

        /* Spacecraft Orientation */
        typedef enum {
            SC_BACKWARD = 0,
            SC_FORWARD = 1,
            SC_TRANSITION = 2
        } sc_orient_t;

        /* Signal Confidence per Photon */
        typedef enum {
            CNF_POSSIBLE_TEP = -2,
            CNF_NOT_CONSIDERED = -1,
            CNF_BACKGROUND = 0,
            CNF_WITHIN_10M = 1,
            CNF_SURFACE_LOW = 2,
            CNF_SURFACE_MEDIUM = 3,
            CNF_SURFACE_HIGH = 4,
            NUM_SIGNAL_CONF = 7,
        } signal_conf_t;

        /* Quality Level per Photon */
        typedef enum {
            QUALITY_NOMINAL = 0,
            QUALITY_POSSIBLE_AFTERPULSE = 1,
            QUALITY_POSSIBLE_IMPULSE_RESPONSE = 2,
            QUALITY_POSSIBLE_TEP = 3,
            NUM_PHOTON_QUALITY = 4,
        } quality_ph_t;

        /* Surface Types for Signal Confidence */
        typedef enum {
            SRT_DYNAMIC = -1, // select surface type with maximum confidence
            SRT_LAND = 0,
            SRT_OCEAN = 1,
            SRT_SEA_ICE = 2,
            SRT_LAND_ICE = 3,
            SRT_INLAND_WATER = 4,
            NUM_SURFACE_TYPES = 5
        } surface_type_t;

        /* ATL08 Surface Classification */
        typedef enum {
            ATL08_NOISE = 0,
            ATL08_GROUND = 1,
            ATL08_CANOPY = 2,
            ATL08_TOP_OF_CANOPY = 3,
            ATL08_UNCLASSIFIED = 4,
            NUM_ATL08_CLASSES = 5
        } atl08_class_t;

        /* Algorithm Stages */
        typedef enum {
            STAGE_ATL06 = 0,    // surface fit
            STAGE_ATL08 = 1,    // use ATL08 photon classifications
            STAGE_YAPC = 2,     // yet another photon classifier
            STAGE_PHOREAL = 3,  // atl08 vegetation science
            NUM_STAGES = 4
        } atl06_stages_t;

        /* Ancillary Field Types */
        typedef enum {
            PHOTON_ANC_TYPE     = 0,
            EXTENT_ANC_TYPE     = 1,
            ATL08_ANC_TYPE      = 2,
            ATL06_ANC_TYPE      = 3
        } anc_type_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int luaCreate (lua_State* L);
        virtual void fromLua (lua_State* L, int index) override;

        /*--------------------------------------------------------------------
         * Inline Methods
         *--------------------------------------------------------------------*/

        // returns nanoseconds since Unix epoch, no leap seconds
        static time8_t deltatime2timestamp (double delta_time)
        {
            return TimeLib::gps2systimeex(delta_time + (double)ATLAS_SDP_EPOCH_GPS);
        }

        // [RGT: 63-52][CYCLE: 51-36][REGION: 35-32][RPT: 31-30][ID: 29-2][PHOTONS|ELEVATION: 1][LEFT|RIGHT: 0]
        static uint64_t generateExtentId (int32_t rgt, int32_t cycle, int32_t region, int track, int pair, uint32_t counter)
        {
            uint64_t extent_id = static_cast<uint64_t>(rgt) << 52 |
                                 static_cast<uint64_t>(cycle) << 36 |
                                 static_cast<uint64_t>(region) << 32 |
                                 static_cast<uint64_t>(track) << 30 |
                                 (static_cast<uint64_t>(counter) & 0xFFFFFFF) << 2 |
                                 static_cast<uint64_t>(pair);

            if(EXTENT_ID_PHOTONS) extent_id |= EXTENT_ID_PHOTONS;

            return extent_id;
        }

        // returns spot number 1 to 6
        static uint8_t getSpotNumber (sc_orient_t sc_orient, track_t track, int pair)
        {
            static const int num_combinations = 18; // 3(number of s/c orientations) * 3(number of tracks) * 2(number of pairs)
            static const spot_t lookup_table [num_combinations] = {
                SPOT_1, // SC_BACKWARD, RPT_1, RPT_L
                SPOT_2, // SC_BACKWARD, RPT_1, RPT_R
                SPOT_3, // SC_BACKWARD, RPT_2, RPT_L
                SPOT_4, // SC_BACKWARD, RPT_2, RPT_R
                SPOT_5, // SC_BACKWARD, RPT_3, RPT_L
                SPOT_6, // SC_BACKWARD, RPT_3, RPT_R
                SPOT_6, // SC_FORWARD, RPT_1, RPT_L
                SPOT_5, // SC_FORWARD, RPT_1, RPT_R
                SPOT_4, // SC_FORWARD, RPT_2, RPT_L
                SPOT_3, // SC_FORWARD, RPT_2, RPT_R
                SPOT_2, // SC_FORWARD, RPT_3, RPT_L
                SPOT_1, // SC_FORWARD, RPT_3, RPT_R
                INVALID_SPOT, // SC_TRANSITION, RPT_1, RPT_L
                INVALID_SPOT, // SC_TRANSITION, RPT_1, RPT_R
                INVALID_SPOT, // SC_TRANSITION, RPT_2, RPT_L
                INVALID_SPOT, // SC_TRANSITION, RPT_2, RPT_R
                INVALID_SPOT, // SC_TRANSITION, RPT_3, RPT_L
                INVALID_SPOT, // SC_TRANSITION, RPT_3, RPT_R
            };
            const int index = (sc_orient * 6) + ((track-1) * 2) + pair;
            return static_cast<uint8_t>(lookup_table[index]);
        }

        // returns spot number 1 to 6
        static uint8_t getSpotNumber (sc_orient_t sc_orient, const char* beam)
        {
            track_t track;
            if(beam[2] == '1') track = RPT_1;
            else if(beam[2] == '2') track = RPT_2;
            else if(beam[2] == '3') track = RPT_3;
            else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam: %s", beam);

            int pair;
            if(beam[3] == 'l') pair = Icesat2Fields::RPT_L;
            else if(beam[3] == 'r') pair = Icesat2Fields::RPT_R;
            else throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam: %s", beam);

            return getSpotNumber(sc_orient, track, pair);
        }

        // returns ground track number 10 - 60
        static uint8_t getGroundTrack (const char* beam)
        {
            if(beam[2] == '1')
            {
                if(beam[3] == 'l') return Icesat2Fields::GT1L;
                else if(beam[3] == 'r') return Icesat2Fields::GT1R;
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam: %s", beam);
            }
            else if(beam[2] == '2')
            {
                if(beam[3] == 'l') return Icesat2Fields::GT2L;
                else if(beam[3] == 'r') return Icesat2Fields::GT2R;
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam: %s", beam);
            }
            else if(beam[2] == '3')
            {
                if(beam[3] == 'l') return Icesat2Fields::GT3L;
                else if(beam[3] == 'r') return Icesat2Fields::GT3R;
                throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam: %s", beam);
            }
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam: %s", beam);
        }

        // returns ground track number 10 - 60
        static uint8_t getGroundTrack (sc_orient_t sc_orient, track_t track, int pair)
        {
            static const int num_combinations = 18; // 3(number of s/c orientations) * 3(number of tracks) * 2(number of pairs)
            static const gt_t lookup_table [num_combinations] = {
                GT1L, // SC_BACKWARD, RPT_1, RPT_L
                GT1R, // SC_BACKWARD, RPT_1, RPT_R
                GT2L, // SC_BACKWARD, RPT_2, RPT_L
                GT2R, // SC_BACKWARD, RPT_2, RPT_R
                GT3L, // SC_BACKWARD, RPT_3, RPT_L
                GT3R, // SC_BACKWARD, RPT_3, RPT_R
                GT1L, // SC_FORWARD, RPT_1, RPT_L
                GT1R, // SC_FORWARD, RPT_1, RPT_R
                GT2L, // SC_FORWARD, RPT_2, RPT_L
                GT2R, // SC_FORWARD, RPT_2, RPT_R
                GT3L, // SC_FORWARD, RPT_3, RPT_L
                GT3R, // SC_FORWARD, RPT_3, RPT_R
                INVALID_GT, // SC_TRANSITION, RPT_1, RPT_L
                INVALID_GT, // SC_TRANSITION, RPT_1, RPT_R
                INVALID_GT, // SC_TRANSITION, RPT_2, RPT_L
                INVALID_GT, // SC_TRANSITION, RPT_2, RPT_R
                INVALID_GT, // SC_TRANSITION, RPT_3, RPT_L
                INVALID_GT, // SC_TRANSITION, RPT_3, RPT_R
            };
            const int index = (sc_orient * 6) + ((track-1) * 2) + pair;
            return static_cast<uint8_t>(lookup_table[index]);
        }

        // returns resource as a string
        const char* getResource (void) const { return resource.value.c_str(); }

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        FieldElement<surface_type_t>                        surfaceType {SRT_DYNAMIC};                              // surface reference type (used to select signal confidence column)
        FieldElement<bool>                                  passInvalid {false};                                    // post extent even if each pair is invalid
        FieldElement<bool>                                  distInSeg {false};                                      // the extent length and step are expressed in segments, not meters
        FieldEnumeration<signal_conf_t,NUM_SIGNAL_CONF>     atl03Cnf {false, false, false, false, true, true, true}; // list of desired signal confidences of photons from atl03 classification
        FieldEnumeration<quality_ph_t,NUM_PHOTON_QUALITY>   qualityPh {true, false, false, false};                  // list of desired photon quality levels from atl03
        FieldEnumeration<atl08_class_t,NUM_ATL08_CLASSES>   atl08Class {false, false, false, false, false};         // list of surface classifications to use (leave empty to skip)
        FieldEnumeration<gt_t,NUM_SPOTS>                    beams {true, true, true, true, true, true};             // list of which beams (gt[l|r][1|2|3])
        FieldElement<int>                                   track {ALL_TRACKS};                                     // reference pair track number (1, 2, 3, or 0 for all tracks)
        FieldElement<int>                                   minPhotonCount {10};                                    // PE
        FieldElement<double>                                minAlongTrackSpread {20.0};                             // meters
        FieldElement<double>                                extentLength {40.0};                                    // length of ATL06 extent (meters or segments if dist_in_seg is true)
        FieldElement<double>                                extentStep {20.0};                                      // resolution of the ATL06 extent (meters or segments if dist_in_seg is true)
        FitFields                                           fit;                                                    // settings used in the surface fitter algorithm
        YapcFields                                          yapc;                                                   // settings used in YAPC algorithm
        PhorealFields                                       phoreal;                                                // phoreal algorithm settings
        Atl24Fields                                         atl24;                                                  // atl24 algorithm settings
        FieldElement<int>                                   maxIterations {5};                                      // DEPRECATED (use FitFields)
        FieldElement<double>                                minWindow {3.0};                                        // DEPRECATED (use FitFields)
        FieldElement<double>                                maxRobustDispersion {5.0};                              // DEPRECATED (use FitFields)
        FieldList<string>                                   atl03GeoFields;                                         // list of geolocation fields to associate with an extent
        FieldList<string>                                   atl03CorrFields;                                        // list of geophys_corr fields to associate with an extent
        FieldList<string>                                   atl03PhFields;                                          // list of per-photon fields to associate with an extent
        FieldList<string>                                   atl06Fields;                                            // list of ATL06 fields to associate with an ATL06 subsetting request
        FieldList<string>                                   atl08Fields;                                            // list of ATL08 fields to associate with an extent
        FieldList<string>                                   atl13Fields;                                            // list of ATL13 fields to associate with an extent
        FieldElement<int>                                   year {-1};                                              // ATL03 granule observation date - year
        FieldElement<int>                                   month {-1};                                             // ATL03 granule observation date - month
        FieldElement<int>                                   day {-1};                                               // ATL03 granule observation date - day
        FieldElement<int>                                   rgt {-1};                                               // ATL03 granule reference ground track
        FieldElement<int>                                   cycle {-1};                                             // ATL03 granule cycle
        FieldElement<int>                                   region {-1};                                            // ATL03 granule region
        FieldElement<int>                                   version {-1};                                           // ATL03 granule version

        bool stages[NUM_STAGES] = {false, false, false, false};

    protected:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                Icesat2Fields   (lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const std::initializer_list<FieldDictionary::init_entry_t>& init_list);
        virtual ~Icesat2Fields  (void) override = default;

        void parseResource (void);
        static int luaStage (lua_State* L);

};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

string convertToJson(const PhorealFields::phoreal_geoloc_t& v);
int convertToLua(lua_State* L, const PhorealFields::phoreal_geoloc_t& v);
void convertFromLua(lua_State* L, int index, PhorealFields::phoreal_geoloc_t& v);

string convertToJson(const Icesat2Fields::signal_conf_t& v);
int convertToLua(lua_State* L, const Icesat2Fields::signal_conf_t& v);
void convertFromLua(lua_State* L, int index, Icesat2Fields::signal_conf_t& v);
int convertToIndex(const Icesat2Fields::signal_conf_t& v);
void convertFromIndex(int index, Icesat2Fields::signal_conf_t& v);

string convertToJson(const Icesat2Fields::quality_ph_t& v);
int convertToLua(lua_State* L, const Icesat2Fields::quality_ph_t& v);
void convertFromLua(lua_State* L, int index, Icesat2Fields::quality_ph_t& v);
int convertToIndex(const Icesat2Fields::quality_ph_t& v);
void convertFromIndex(int index, Icesat2Fields::quality_ph_t& v);

string convertToJson(const Icesat2Fields::atl08_class_t& v);
int convertToLua(lua_State* L, const Icesat2Fields::atl08_class_t& v);
void convertFromLua(lua_State* L, int index, Icesat2Fields::atl08_class_t& v);
int convertToIndex(const Icesat2Fields::atl08_class_t& v);
void convertFromIndex(int index, Icesat2Fields::atl08_class_t& v);

string convertToJson(const Icesat2Fields::gt_t& v);
int convertToLua(lua_State* L, const Icesat2Fields::gt_t& v);
void convertFromLua(lua_State* L, int index, Icesat2Fields::gt_t& v);
int convertToIndex(const Icesat2Fields::gt_t& v);
void convertFromIndex(int index, Icesat2Fields::gt_t& v);

string convertToJson(const Icesat2Fields::spot_t& v);
int convertToLua(lua_State* L, const Icesat2Fields::spot_t& v);
void convertFromLua(lua_State* L, int index, Icesat2Fields::spot_t& v);
int convertToIndex(const Icesat2Fields::spot_t& v);
void convertFromIndex(int index, Icesat2Fields::spot_t& v);

string convertToJson(const Icesat2Fields::surface_type_t& v);
int convertToLua(lua_State* L, const Icesat2Fields::surface_type_t& v);
void convertFromLua(lua_State* L, int index, Icesat2Fields::surface_type_t& v);

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

inline uint32_t toEncoding(Icesat2Fields::surface_type_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Icesat2Fields::spot_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Icesat2Fields::gt_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Icesat2Fields::atl08_class_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Icesat2Fields::quality_ph_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Icesat2Fields::signal_conf_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(PhorealFields::phoreal_geoloc_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Atl24Fields::class_t& v) { (void)v; return Field::INT32; }
inline uint32_t toEncoding(Atl24Fields::flag_t& v) { (void)v; return Field::INT32; }

#endif  /* __icesat2_fields__ */
