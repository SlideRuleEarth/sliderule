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

#ifndef __icesat2__
#define __icesat2__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Atl03Device.h"
#include "Atl06Dispatch.h"
#include "GTArray.h"
#include "UT_Atl06Dispatch.h"

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

#define ATL06_DEFAULT_SURFACE_TYPE              SRT_LAND_ICE        
#define ATL06_DEFAULT_SIGNAL_CONFIDENCE         CNF_SURFACE_HIGH
#define ATL06_DEFAULT_ALONG_TRACK_SPREAD        20.0 // meters
#define ATL06_DEFAULT_MIN_PHOTON_COUNT          10
#define ATL06_DEFAULT_EXTENT_LENGTH             40.0 // meters
#define ATL06_DEFAULT_EXTENT_STEP               20.0 // meters
#define ATL06_DEFAULT_MAX_ITERATIONS            20
#define ATL06_DEFAULT_MIN_WINDOW                3.0 // meters
#define ATL06_DEFAULT_MAX_ROBUST_DISPERSION     5.0 // meters

/******************************************************************************
 * PROTOTYPES
 ******************************************************************************/

extern "C" {
void initicesat2 (void);
}

#endif  /* __icesat2__ */


