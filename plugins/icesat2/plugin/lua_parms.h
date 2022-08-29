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

#ifndef __lua_parms__
#define __lua_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <lua.h>
#include "GeoJsonRaster.h"
#include "List.h"
#include "MathLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_SURFACE_TYPE                   "srt"
#define LUA_PARM_ATL03_CNF                      "cnf"
#define LUA_PARM_YAPC                           "yapc"
#define LUA_PARM_YAPC_SCORE                     "score"
#define LUA_PARM_YAPC_KNN                       "knn"
#define LUA_PARM_YAPC_MIN_KNN                   "min_knn"
#define LUA_PARM_YAPC_WIN_H                     "win_h"
#define LUA_PARM_YAPC_WIN_X                     "win_x"
#define LUA_PARM_YAPC_VERSION                   "version"
#define LUA_PARM_ATL08_CLASS                    "atl08_class"
#define LUA_PARM_QUALITY                        "quality_ph"
#define LUA_PARM_POLYGON                        "poly"
#define LUA_PARM_RASTER                         "file"
#define LUA_PARM_TRACK                          "track"
#define LUA_PARM_STAGES                         "stages"
#define LUA_PARM_COMPACT                        "compact"
#define LUA_PARM_LATITUDE                       "lat"
#define LUA_PARM_LONGITUDE                      "lon"
#define LUA_PARM_ALONG_TRACK_SPREAD             "ats"
#define LUA_PARM_MIN_PHOTON_COUNT               "cnt"
#define LUA_PARM_EXTENT_LENGTH                  "len"
#define LUA_PARM_EXTENT_STEP                    "res"
#define LUA_PARM_MAX_ITERATIONS                 "maxi"
#define LUA_PARM_MIN_WINDOW                     "H_min_win"
#define LUA_PARM_MAX_ROBUST_DISPERSION          "sigma_r_max"
#define LUA_PARM_PASS_INVALID                   "pass_invalid"
#define LUA_PARM_DISTANCE_IN_SEGMENTS           "dist_in_seg"
#define LUA_PARM_ATL08_CLASS_NOISE              "atl08_noise"
#define LUA_PARM_ATL08_CLASS_GROUND             "atl08_ground"
#define LUA_PARM_ATL08_CLASS_CANOPY             "atl08_canopy"
#define LUA_PARM_ATL08_CLASS_TOP_OF_CANOPY      "atl08_top_of_canopy"
#define LUA_PARM_ATL08_CLASS_UNCLASSIFIED       "atl08_unclassified"
#define LUA_PARM_ATL03_CNF_TEP                  "atl03_tep"
#define LUA_PARM_ATL03_CNF_NOT_CONSIDERED       "atl03_not_considered"
#define LUA_PARM_ATL03_CNF_BACKGROUND           "atl03_background"
#define LUA_PARM_ATL03_CNF_WITHIN_10M           "atl03_within_10m"
#define LUA_PARM_ATL03_CNF_LOW                  "atl03_low"
#define LUA_PARM_ATL03_CNF_MEDIUM               "atl03_medium"
#define LUA_PARM_ATL03_CNF_HIGH                 "atl03_high"
#define LUA_PARM_QUALITY_NOMINAL                "atl03_quality_nominal"
#define LUA_PARM_QUALITY_AFTERPULSE             "atl03_quality_afterpulse"
#define LUA_PARM_QUALITY_IMPULSE_RESPONSE       "atl03_quality_impulse_response"
#define LUA_PARM_QUALITY_TEP                    "atl03_quality_tep"

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

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
    GT1L = 10,
    GT1R = 20,
    GT2L = 30,
    GT2R = 40,
    GT3L = 50,
    GT3R = 60
} gt_t;

/* Spots */
typedef enum {
    SPOT_1 = 1,
    SPOT_2 = 2,
    SPOT_3 = 3,
    SPOT_4 = 4,
    SPOT_5 = 5,
    SPOT_6 = 6,
    NUM_SPOTS = 6
} spot_t;

/* Spacecraft Orientation */
typedef enum {
    SC_BACKWARD = 0,
    SC_FORWARD = 1,
    SC_TRANSITION = 2
} sc_orient_t;

/* Signal Confidence per Photon */
typedef enum {
    SIGNAL_CONF_OFFSET = 2, // added to value to get index
    CNF_POSSIBLE_TEP = -2,
    CNF_NOT_CONSIDERED = -1,
    CNF_BACKGROUND = 0,
    CNF_WITHIN_10M = 1,
    CNF_SURFACE_LOW = 2,
    CNF_SURFACE_MEDIUM = 3,
    CNF_SURFACE_HIGH = 4,
    NUM_SIGNAL_CONF = 7,
    ATL03_INVALID_CONFIDENCE = 8
} signal_conf_t;

/* Quality Level per Photon */
typedef enum {
    QUALITY_NOMINAL = 0,
    QUALITY_POSSIBLE_AFTERPULSE = 1,
    QUALITY_POSSIBLE_IMPULSE_RESPONSE = 2,
    QUALITY_POSSIBLE_TEP = 3,
    NUM_PHOTON_QUALITY = 4,
    ATL03_INVALID_QUALITY = 5
} quality_ph_t;

/* Surface Types for Signal Confidence */
typedef enum {
    SRT_LAND = 0,
    SRT_OCEAN = 1,
    SRT_SEA_ICE = 2,
    SRT_LAND_ICE = 3,
    SRT_INLAND_WATER = 4
} surface_type_t;

/* ATL08 Surface Classification */
typedef enum {
    ATL08_NOISE = 0,
    ATL08_GROUND = 1,
    ATL08_CANOPY = 2,
    ATL08_TOP_OF_CANOPY = 3,
    ATL08_UNCLASSIFIED = 4,
    NUM_ATL08_CLASSES = 5,
    ATL08_INVALID_CLASSIFICATION = 6
} atl08_classification_t;

/* YAPC Settings */
typedef struct {
    uint8_t score;      // minimum allowed weight of photon using yapc algorithm
    int     version;    // version of the yapc algorithm to run
    int     knn;        // k-nearest neighbor
    int     min_knn;    // minimum number of k-nearest neighors
    double  win_h;      // window height (overrides calculated value if non-zero)
    double  win_x;      // window width
} yapc_t;

/* Algorithm Stages */
typedef enum {
    STAGE_LSF = 0,      // least squares fit
    STAGE_ATL08 = 1,    // use ATL08 photon classifications
    STAGE_YAPC = 2,     // yet another photon classifier
    NUM_STAGES = 3
} atl06_stages_t;

/* Extraction Parameters */
typedef struct {
    surface_type_t          surface_type;                   // surface reference type (used to select signal confidence column)
    bool                    pass_invalid;                   // post extent even if each pair is invalid
    bool                    dist_in_seg;                    // the extent length and step are expressed in segments, not meters
    bool                    compact;                        // return compact (only lat,lon,height,time) elevation information
    bool                    atl03_cnf[NUM_SIGNAL_CONF];     // list of desired signal confidences of photons from atl03 classification
    bool                    quality_ph[NUM_PHOTON_QUALITY]; // list of desired photon quality levels from atl03
    bool                    atl08_class[NUM_ATL08_CLASSES]; // list of surface classifications to use (leave empty to skip)
    bool                    stages[NUM_STAGES];             // algorithm iterations
    yapc_t                  yapc;                           // settings used in YAPC algorithm
    List<MathLib::coord_t>  polygon;                        // polygon of region of interest
    GeoJsonRaster*          raster;                         // raster of region of interest, created from geojson file
    int                     track;                          // reference pair track number (1, 2, 3, or 0 for all tracks)
    int                     max_iterations;                 // least squares fit iterations
    int                     minimum_photon_count;           // PE
    double                  along_track_spread;             // meters
    double                  minimum_window;                 // H_win minimum
    double                  maximum_robust_dispersion;      // sigma_r
    double                  extent_length;                  // length of ATL06 extent (meters or segments if dist_in_seg is true)
    double                  extent_step;                    // resolution of the ATL06 extent (meters or segments if dist_in_seg is true)
} atl06_parms_t;

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

atl06_parms_t* getLuaAtl06Parms (lua_State* L, int index);

#endif  /* __lua_parms__ */
