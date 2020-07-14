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

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "lua_parms.h"
#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

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
 * FILE DATA
 ******************************************************************************/

const atl06_parms_t DefaultParms = {
    .surface_type               = ATL06_DEFAULT_SURFACE_TYPE,
    .signal_confidence          = ATL06_DEFAULT_SIGNAL_CONFIDENCE,
    .stages                     = { true },
    .max_iterations             = ATL06_DEFAULT_MAX_ITERATIONS,
    .along_track_spread         = ATL06_DEFAULT_ALONG_TRACK_SPREAD,
    .minimum_photon_count       = ATL06_DEFAULT_MIN_PHOTON_COUNT,
    .minimum_window             = ATL06_DEFAULT_MIN_WINDOW,
    .maximum_robust_dispersion  = ATL06_DEFAULT_MAX_ROBUST_DISPERSION,
    .extent_length              = ATL06_DEFAULT_EXTENT_LENGTH,
    .extent_step                = ATL06_DEFAULT_EXTENT_STEP
};

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

atl06_parms_t lua_parms_process (lua_State* L, int index)
{
    atl06_parms_t parms = DefaultParms;

    if(lua_type(L, index) == LUA_TTABLE)
    {
        bool provided = false;

        lua_getfield(L, 2, LUA_PARM_SURFACE_TYPE);
        parms.surface_type = (surface_type_t)LuaObject::getLuaInteger(L, -1, true, parms.surface_type, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %d\n", LUA_PARM_SURFACE_TYPE, (int)parms.surface_type);

        lua_getfield(L, 2, LUA_PARM_SIGNAL_CONFIDENCE);
        parms.signal_confidence = (signal_conf_t)LuaObject::getLuaInteger(L, -1, true, parms.signal_confidence, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %d\n", LUA_PARM_SIGNAL_CONFIDENCE, (int)parms.signal_confidence);

        lua_getfield(L, 2, LUA_PARM_MAX_ITERATIONS);
        parms.max_iterations = LuaObject::getLuaInteger(L, -1, true, parms.max_iterations, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %d\n", LUA_PARM_MAX_ITERATIONS, (int)parms.max_iterations);

        lua_getfield(L, 2, LUA_PARM_ALONG_TRACK_SPREAD);
        parms.along_track_spread = LuaObject::getLuaFloat(L, -1, true, parms.along_track_spread, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_ALONG_TRACK_SPREAD, parms.along_track_spread);

        lua_getfield(L, 2, LUA_PARM_MIN_PHOTON_COUNT);
        parms.minimum_photon_count = LuaObject::getLuaInteger(L, -1, true, parms.minimum_photon_count, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_MIN_PHOTON_COUNT, parms.minimum_photon_count);

        lua_getfield(L, 2, LUA_PARM_MIN_WINDOW);
        parms.minimum_window = LuaObject::getLuaFloat(L, -1, true, parms.minimum_window, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_MIN_WINDOW, parms.minimum_window);

        lua_getfield(L, 2, LUA_PARM_MAX_ROBUST_DISPERSION);
        parms.maximum_robust_dispersion = LuaObject::getLuaFloat(L, -1, true, parms.maximum_robust_dispersion, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_MAX_ROBUST_DISPERSION, parms.maximum_robust_dispersion);

        lua_getfield(L, 2, LUA_PARM_EXTENT_LENGTH);
        parms.extent_length = LuaObject::getLuaFloat(L, -1, true, parms.extent_length, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_EXTENT_LENGTH, parms.extent_length);

        lua_getfield(L, 2, LUA_PARM_EXTENT_STEP);
        parms.extent_step = LuaObject::getLuaFloat(L, -1, true, parms.extent_step, &provided);
        if(provided) mlog(CRITICAL, "Setting %s to %lf\n", LUA_PARM_EXTENT_STEP, parms.extent_step);
    }

    return parms;
}