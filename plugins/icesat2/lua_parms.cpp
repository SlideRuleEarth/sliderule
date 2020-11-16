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
    .stages                     = { false, true },
    .polygon                    = { { 0, 0 } },
    .points_in_polygon          = 0,
    .max_iterations             = ATL06_DEFAULT_MAX_ITERATIONS,
    .along_track_spread         = ATL06_DEFAULT_ALONG_TRACK_SPREAD,
    .minimum_photon_count       = ATL06_DEFAULT_MIN_PHOTON_COUNT,
    .minimum_window             = ATL06_DEFAULT_MIN_WINDOW,
    .maximum_robust_dispersion  = ATL06_DEFAULT_MAX_ROBUST_DISPERSION,
    .extent_length              = ATL06_DEFAULT_EXTENT_LENGTH,
    .extent_step                = ATL06_DEFAULT_EXTENT_STEP
};

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

void get_lua_polygon (lua_State* L, int index, atl06_parms_t* parms, bool* provided)
{
    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        /* Get Number of Points in Polygon */
        int num_points = lua_rawlen(L, index);
        if(num_points > LUA_PARM_MAX_COORDS)
        {
            mlog(CRITICAL, "Points in polygon [%d] exceed maximum: %d\n", num_points, LUA_PARM_MAX_COORDS);
            num_points = LUA_PARM_MAX_COORDS;
        }
        parms->points_in_polygon = num_points;

        /* Iterate through each coordinate */
        for(int i = 0; i < num_points; i++)
        {
            /* Get coordinate table */
            lua_rawgeti(L, index, i+1);
            if(lua_istable(L, index))
            {
                MathLib::coord_t coord;

                /* Get latitude entry */
                lua_getfield(L, index, LUA_PARM_LATITUDE);
                coord.lat = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Get longitude entry */
                lua_getfield(L, index, LUA_PARM_LONGITUDE);
                coord.lon = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Add Coordinate */
                parms->polygon[i] = coord;
                if(provided) *provided = true;
            }
            lua_pop(L, 1);
        }
    }
}

void get_lua_stages (lua_State* L, int index, atl06_parms_t* parms, bool* provided)
{
    /* Must be table of stages */
    if(lua_istable(L, index))
    {
        /* Clear stages table (sets all to false) */
        LocalLib::set(parms->stages, 0, sizeof(parms->stages));

        /* Get number of stages in table */
        int num_stages = lua_rawlen(L, index);
        if(num_stages > 0 && provided) *provided = true;

        /* Iterate through each stage in table */
        for(int i = 0; i < num_stages; i++)
        {
            /* Get stage */
            lua_rawgeti(L, index, i+1);

            /* Set stage */
            if(lua_isinteger(L, -1))
            {
                int stage = LuaObject::getLuaInteger(L, -1);
                if(stage >= 0 && stage < NUM_STAGES)
                {
                    parms->stages[stage] = true;
                }
            }
            else if(lua_isstring(L, -1))
            {
                const char* stage_str = LuaObject::getLuaString(L, -1);
                if(StringLib::match(stage_str, LUA_PARM_STAGE_LSF))
                {
                    parms->stages[STAGE_LSF] = true;
                    mlog(INFO, "Enabling %s stage\n", LUA_PARM_STAGE_LSF);
                }
                else if(StringLib::match(stage_str, LUA_PARM_STAGE_RAW))
                {
                    parms->stages[STAGE_RAW] = true;
                    mlog(INFO, "Enabling %s stage\n", LUA_PARM_STAGE_RAW);
                }
                else if(StringLib::match(stage_str, LUA_PARM_STAGE_SUB))
                {
                    parms->stages[STAGE_SUB] = true;
                    mlog(INFO, "Enabling %s stage\n", LUA_PARM_STAGE_SUB);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

atl06_parms_t getLuaAtl06Parms (lua_State* L, int index)
{
    atl06_parms_t parms = DefaultParms;

    if(lua_type(L, index) == LUA_TTABLE)
    {
        bool provided = false;

        lua_getfield(L, index, LUA_PARM_SURFACE_TYPE);
        parms.surface_type = (surface_type_t)LuaObject::getLuaInteger(L, -1, true, parms.surface_type, &provided);
        if(provided) mlog(INFO, "Setting %s to %d\n", LUA_PARM_SURFACE_TYPE, (int)parms.surface_type);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_SIGNAL_CONFIDENCE);
        parms.signal_confidence = (signal_conf_t)LuaObject::getLuaInteger(L, -1, true, parms.signal_confidence, &provided);
        if(provided) mlog(INFO, "Setting %s to %d\n", LUA_PARM_SIGNAL_CONFIDENCE, (int)parms.signal_confidence);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_POLYGON);
        get_lua_polygon(L, -1, &parms, &provided);
        if(provided) mlog(INFO, "Setting %s to %d points\n", LUA_PARM_POLYGON, (int)parms.points_in_polygon);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_STAGES);
        get_lua_stages(L, -1, &parms, &provided);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_MAX_ITERATIONS);
        parms.max_iterations = LuaObject::getLuaInteger(L, -1, true, parms.max_iterations, &provided);
        if(provided) mlog(INFO, "Setting %s to %d\n", LUA_PARM_MAX_ITERATIONS, (int)parms.max_iterations);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_ALONG_TRACK_SPREAD);
        parms.along_track_spread = LuaObject::getLuaFloat(L, -1, true, parms.along_track_spread, &provided);
        if(provided) mlog(INFO, "Setting %s to %lf\n", LUA_PARM_ALONG_TRACK_SPREAD, parms.along_track_spread);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_MIN_PHOTON_COUNT);
        parms.minimum_photon_count = LuaObject::getLuaInteger(L, -1, true, parms.minimum_photon_count, &provided);
        if(provided) mlog(INFO, "Setting %s to %lf\n", LUA_PARM_MIN_PHOTON_COUNT, parms.minimum_photon_count);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_MIN_WINDOW);
        parms.minimum_window = LuaObject::getLuaFloat(L, -1, true, parms.minimum_window, &provided);
        if(provided) mlog(INFO, "Setting %s to %lf\n", LUA_PARM_MIN_WINDOW, parms.minimum_window);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_MAX_ROBUST_DISPERSION);
        parms.maximum_robust_dispersion = LuaObject::getLuaFloat(L, -1, true, parms.maximum_robust_dispersion, &provided);
        if(provided) mlog(INFO, "Setting %s to %lf\n", LUA_PARM_MAX_ROBUST_DISPERSION, parms.maximum_robust_dispersion);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_EXTENT_LENGTH);
        parms.extent_length = LuaObject::getLuaFloat(L, -1, true, parms.extent_length, &provided);
        if(provided) mlog(INFO, "Setting %s to %lf\n", LUA_PARM_EXTENT_LENGTH, parms.extent_length);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_EXTENT_STEP);
        parms.extent_step = LuaObject::getLuaFloat(L, -1, true, parms.extent_step, &provided);
        if(provided) mlog(INFO, "Setting %s to %lf\n", LUA_PARM_EXTENT_STEP, parms.extent_step);
        lua_pop(L, 1);
    }

    return parms;
}