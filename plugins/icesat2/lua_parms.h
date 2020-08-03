/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef __lua_parms__
#define __lua_parms__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include <lua.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define LUA_PARM_SURFACE_TYPE                   "srt"
#define LUA_PARM_SIGNAL_CONFIDENCE              "cnf"
#define LUA_PARM_ALONG_TRACK_SPREAD             "ats"
#define LUA_PARM_MIN_PHOTON_COUNT               "cnt"
#define LUA_PARM_EXTENT_LENGTH                  "len"
#define LUA_PARM_EXTENT_STEP                    "res"
#define LUA_PARM_MAX_ITERATIONS                 "maxi"
#define LUA_PARM_MIN_WINDOW                     "H_min_win"
#define LUA_PARM_MAX_ROBUST_DISPERSION          "sigma_r_max"

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

/* Tracks */
typedef enum {
    ALL_TRACKS = 0,
    RPT_1 = 1,
    RPT_2 = 2,
    RPT_3 = 3,
    NUM_TRACKS = 3
} track_t;

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
    CNF_POSSIBLE_TEP = -2,
    CNF_NOT_CONSIDERED = -1,
    CNF_BACKGROUND = 0,
    CNF_WITHIN_10M = 1,
    CNF_SURFACE_LOW = 2,
    CNF_SURFACE_MEDIUM = 3,
    CNF_SURFACE_HIGH = 4
} signal_conf_t;

/* Surface Types for Signal Confidence */
typedef enum {
    SRT_LAND = 0,
    SRT_OCEAN = 1,
    SRT_SEA_ICE = 2,
    SRT_LAND_ICE = 3,
    SRT_INLAND_WATER = 4
} surface_type_t;

/* Algorithm Stages */
typedef enum {
    STAGE_LSF = 0,  // least squares fit
    NUM_STAGES = 1
} atl06_stages_t;

/* Extraction Parameters */
typedef struct {
    surface_type_t  surface_type;               // surface reference type (used to select signal confidence column)
    signal_conf_t   signal_confidence;          // minimal allowed signal confidence
    bool            stages[NUM_STAGES];         // algorithm iterations
    int             max_iterations;             // least squares fit iterations
    double          along_track_spread;         // meters
    double          minimum_photon_count;       // PE
    double          minimum_window;             // H_win minimum
    double          maximum_robust_dispersion;  // sigma_r
    double          extent_length;              // length of ATL06 extent (meters)
    double          extent_step;                // resolution of the ATL06 extent (meters)
} atl06_parms_t;

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

atl06_parms_t lua_parms_process (lua_State* L, int index);

#endif  /* __lua_parms__ */


