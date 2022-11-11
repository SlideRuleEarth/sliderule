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

/******************************************************************************
 * INCLUDE
 ******************************************************************************/

#include "core.h"
#include "lua_parms.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define ATL06_DEFAULT_SURFACE_TYPE              SRT_LAND_ICE
#define ATL06_DEFAULT_SIGNAL_CONFIDENCE         CNF_SURFACE_LOW
#define ATL06_DEFAULT_YAPC_SCORE                0
#define ATL06_DEFAULT_YAPC_VERSION              3
#define ALT06_DEFAULT_YAPC_WIN_X                15.0
#define ALT06_DEFAULT_YAPC_WIN_H                6.0
#define ALT06_DEFAULT_YAPC_MIN_KNN              5
#define ATL06_DEFAULT_ALONG_TRACK_SPREAD        20.0 // meters
#define ATL06_DEFAULT_MIN_PHOTON_COUNT          10
#define ATL06_DEFAULT_EXTENT_LENGTH             40.0 // meters
#define ATL06_DEFAULT_EXTENT_STEP               20.0 // meters
#define ATL06_DEFAULT_MAX_ITERATIONS            5
#define ATL06_DEFAULT_MIN_WINDOW                3.0 // meters
#define ATL06_DEFAULT_MAX_ROBUST_DISPERSION     5.0 // meters
#define ATL06_DEFAULT_COMPACT                   false
#define ATL06_DEFAULT_PASS_INVALID              false
#define ATL06_DEFAULT_DIST_IN_SEG               false

/******************************************************************************
 * FILE DATA
 ******************************************************************************/

const icesat2_parms_t DefaultParms = {
    .surface_type               = ATL06_DEFAULT_SURFACE_TYPE,
    .pass_invalid               = ATL06_DEFAULT_PASS_INVALID,
    .dist_in_seg                = ATL06_DEFAULT_DIST_IN_SEG,
    .compact                    = ATL06_DEFAULT_COMPACT,
    .atl03_cnf                  = { false, false, true, true, true, true, true },
    .quality_ph                 = { true, false, false, false },
    .atl08_class                = { false, false, false, false, false },
    .stages                     = { true, false, false },
    .yapc                       = { .score = ATL06_DEFAULT_YAPC_SCORE,
                                    .version = ATL06_DEFAULT_YAPC_VERSION,
                                    .knn = 0, // calculated by default
                                    .min_knn = ALT06_DEFAULT_YAPC_MIN_KNN,
                                    .win_h = ALT06_DEFAULT_YAPC_WIN_H,
                                    .win_x = ALT06_DEFAULT_YAPC_WIN_X },
    .raster                     = NULL,
    .track                      = ALL_TRACKS,
    .max_iterations             = ATL06_DEFAULT_MAX_ITERATIONS,
    .minimum_photon_count       = ATL06_DEFAULT_MIN_PHOTON_COUNT,
    .along_track_spread         = ATL06_DEFAULT_ALONG_TRACK_SPREAD,
    .minimum_window             = ATL06_DEFAULT_MIN_WINDOW,
    .maximum_robust_dispersion  = ATL06_DEFAULT_MAX_ROBUST_DISPERSION,
    .extent_length              = ATL06_DEFAULT_EXTENT_LENGTH,
    .extent_step                = ATL06_DEFAULT_EXTENT_STEP,
    .atl03_geolocation_fields   = NULL,
    .atl03_geocorrection_fields = NULL,
    .atl03_height_fields        = NULL,
    .atl08_signal_photon_fields = NULL
};

/******************************************************************************
 * LOCAL FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * str2atl03cnf
 *----------------------------------------------------------------------------*/
static signal_conf_t str2atl03cnf (const char* confidence_str)
{
    if     (StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_TEP))               return CNF_POSSIBLE_TEP;
    else if(StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_NOT_CONSIDERED))    return CNF_NOT_CONSIDERED;
    else if(StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_BACKGROUND))        return CNF_BACKGROUND;
    else if(StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_WITHIN_10M))        return CNF_WITHIN_10M;
    else if(StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_LOW))               return CNF_SURFACE_LOW;
    else if(StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_MEDIUM))            return CNF_SURFACE_MEDIUM;
    else if(StringLib::match(confidence_str, LUA_PARM_ATL03_CNF_HIGH))              return CNF_SURFACE_HIGH;
    else                                                                            return ATL03_INVALID_CONFIDENCE;
}

/*----------------------------------------------------------------------------
 * str2atl03quality
 *----------------------------------------------------------------------------*/
static quality_ph_t str2atl03quality (const char* quality_ph_str)
{
    if     (StringLib::match(quality_ph_str, LUA_PARM_QUALITY_NOMINAL))             return QUALITY_NOMINAL;
    else if(StringLib::match(quality_ph_str, LUA_PARM_QUALITY_AFTERPULSE))          return QUALITY_POSSIBLE_AFTERPULSE;
    else if(StringLib::match(quality_ph_str, LUA_PARM_QUALITY_IMPULSE_RESPONSE))    return QUALITY_POSSIBLE_IMPULSE_RESPONSE;
    else if(StringLib::match(quality_ph_str, LUA_PARM_QUALITY_TEP))                 return QUALITY_POSSIBLE_TEP;
    else                                                                            return ATL03_INVALID_QUALITY;
}

/*----------------------------------------------------------------------------
 * str2atl08class
 *----------------------------------------------------------------------------*/
static atl08_classification_t str2atl08class (const char* classifiction_str)
{
    if     (StringLib::match(classifiction_str, LUA_PARM_ATL08_CLASS_NOISE))            return ATL08_NOISE;
    else if(StringLib::match(classifiction_str, LUA_PARM_ATL08_CLASS_GROUND))           return ATL08_GROUND;
    else if(StringLib::match(classifiction_str, LUA_PARM_ATL08_CLASS_CANOPY))           return ATL08_CANOPY;
    else if(StringLib::match(classifiction_str, LUA_PARM_ATL08_CLASS_TOP_OF_CANOPY))    return ATL08_TOP_OF_CANOPY;
    else if(StringLib::match(classifiction_str, LUA_PARM_ATL08_CLASS_UNCLASSIFIED))     return ATL08_UNCLASSIFIED;
    else                                                                                return ATL08_INVALID_CLASSIFICATION;
}

/*----------------------------------------------------------------------------
 * get_lua_atl03_cnf
 *----------------------------------------------------------------------------*/
static void get_lua_atl03_cnf (lua_State* L, int index, icesat2_parms_t* parms, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of classifications or a single classification as a string */
    if(lua_istable(L, index))
    {
        /* Clear confidence table (sets all to false) */
        LocalLib::set(parms->atl03_cnf, 0, sizeof(parms->atl03_cnf));

        /* Get number of classifications in table */
        int num_cnf = lua_rawlen(L, index);
        if(num_cnf > 0 && provided) *provided = true;

        /* Iterate through each classification in table */
        for(int i = 0; i < num_cnf; i++)
        {
            /* Get confidence */
            lua_rawgeti(L, index, i+1);

            /* Set confidence */
            if(lua_isinteger(L, -1))
            {
                int confidence = LuaObject::getLuaInteger(L, -1);
                if(confidence >= CNF_POSSIBLE_TEP && confidence <= CNF_SURFACE_HIGH)
                {
                    parms->atl03_cnf[confidence + SIGNAL_CONF_OFFSET] = true;
                    mlog(DEBUG, "Selecting confidence %d", confidence);
                }
                else
                {
                    mlog(ERROR, "Invalid ATL03 confidence: %d", confidence);
                }
            }
            else if(lua_isstring(L, -1))
            {
                const char* confidence_str = LuaObject::getLuaString(L, -1);
                signal_conf_t confidence = str2atl03cnf(confidence_str);
                if(confidence != ATL03_INVALID_CONFIDENCE)
                {
                    parms->atl03_cnf[confidence + SIGNAL_CONF_OFFSET] = true;
                    mlog(DEBUG, "Selecting %s confidence", confidence_str);
                }
                else
                {
                    mlog(ERROR, "Invalid ATL03 confidence: %s", confidence_str);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(lua_isinteger(L, index))
    {
        /* Clear confidence table (sets all to false) */
        LocalLib::set(parms->atl03_cnf, 0, sizeof(parms->atl03_cnf));

        /* Set Confidences */
        int confidence = LuaObject::getLuaInteger(L, index);
        if(confidence >= CNF_POSSIBLE_TEP && confidence <= CNF_SURFACE_HIGH)
        {
            if(provided) *provided = true;
            for(int c = confidence; c <= CNF_SURFACE_HIGH; c++)
            {
                parms->atl03_cnf[c + SIGNAL_CONF_OFFSET] = true;
                mlog(DEBUG, "Selecting confidence %d", c);
            }
        }
        else
        {
            mlog(ERROR, "Invalid ATL03 confidence: %d", confidence);
        }
    }
    else if(lua_isstring(L, index))
    {
        /* Clear confidence table (sets all to false) */
        LocalLib::set(parms->atl03_cnf, 0, sizeof(parms->atl03_cnf));

        /* Set Confidences */
        const char* confidence_str = LuaObject::getLuaString(L, index);
        signal_conf_t confidence = str2atl03cnf(confidence_str);
        if(confidence != ATL03_INVALID_CONFIDENCE)
        {
            if(provided) *provided = true;
            for(int c = confidence; c <= CNF_SURFACE_HIGH; c++)
            {
                parms->atl03_cnf[c + SIGNAL_CONF_OFFSET] = true;
                mlog(DEBUG, "Selecting confidence %d", c);
            }
        }
        else
        {
            mlog(ERROR, "Invalid ATL03 confidence: %s", confidence_str);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "ATL03 confidence must be provided as a table or string");
    }
}

/*----------------------------------------------------------------------------
 * get_lua_atl03_quality
 *----------------------------------------------------------------------------*/
static void get_lua_atl03_quality (lua_State* L, int index, icesat2_parms_t* parms, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of photon qualities or a single quality as a string */
    if(lua_istable(L, index))
    {
        /* Clear photon quality table (sets all to false) */
        LocalLib::set(parms->quality_ph, 0, sizeof(parms->quality_ph));

        /* Get number of photon quality level in table */
        int num_quality = lua_rawlen(L, index);
        if(num_quality > 0 && provided) *provided = true;

        /* Iterate through each photon quality level in table */
        for(int i = 0; i < num_quality; i++)
        {
            /* Get photon quality */
            lua_rawgeti(L, index, i+1);

            /* Set photon quality */
            if(lua_isinteger(L, -1))
            {
                int quality = LuaObject::getLuaInteger(L, -1);
                if(quality >= QUALITY_NOMINAL && quality <= QUALITY_POSSIBLE_TEP)
                {
                    parms->quality_ph[quality] = true;
                    mlog(DEBUG, "Selecting photon quality %d", quality);
                }
                else
                {
                    mlog(ERROR, "Invalid ATL03 photon quality: %d", quality);
                }
            }
            else if(lua_isstring(L, -1))
            {
                const char* quality_ph_str = LuaObject::getLuaString(L, -1);
                quality_ph_t quality = str2atl03quality(quality_ph_str);
                if(quality != ATL03_INVALID_QUALITY)
                {
                    parms->quality_ph[quality] = true;
                    mlog(DEBUG, "Selecting %s photon quality", quality_ph_str);
                }
                else
                {
                    mlog(ERROR, "Invalid ATL03 photon quality: %s", quality_ph_str);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(lua_isinteger(L, index))
    {
        /* Clear photon quality table (sets all to false) */
        LocalLib::set(parms->quality_ph, 0, sizeof(parms->quality_ph));

        /* Set Photon Quality Level */
        int quality = LuaObject::getLuaInteger(L, index);
        if(quality >= QUALITY_NOMINAL && quality <= QUALITY_POSSIBLE_TEP)
        {
            if(provided) *provided = true;
            for(int q = quality; q <= QUALITY_POSSIBLE_TEP; q++)
            {
                parms->quality_ph[q] = true;
                mlog(DEBUG, "Selecting photon quality %d", q);
            }
        }
        else
        {
            mlog(ERROR, "Invalid ATL03 photon quality: %d", quality);
        }
    }
    else if(lua_isstring(L, index))
    {
        /* Clear photon quality table (sets all to false) */
        LocalLib::set(parms->quality_ph, 0, sizeof(parms->quality_ph));

        /* Set Photon Quality Level */
        const char* quality_ph_str = LuaObject::getLuaString(L, index);
        quality_ph_t quality = str2atl03quality(quality_ph_str);
        if(quality != ATL03_INVALID_QUALITY)
        {
            if(provided) *provided = true;
            for(int q = quality; q <= QUALITY_POSSIBLE_TEP; q++)
            {
                parms->quality_ph[q] = true;
                mlog(DEBUG, "Selecting photon quality %d", q);
            }
        }
        else
        {
            mlog(ERROR, "Invalid ATL03 photon quality: %s", quality_ph_str);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "ATL03 photon quality must be provided as a table or string");
    }
}

/*----------------------------------------------------------------------------
 * get_lua_atl08_class
 *----------------------------------------------------------------------------*/
static void get_lua_atl08_class (lua_State* L, int index, icesat2_parms_t* parms, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of classifications or a single classification as a string */
    if(lua_istable(L, index))
    {
        /* Clear classification table (sets all to false) */
        LocalLib::set(parms->atl08_class, 0, sizeof(parms->atl08_class));

        /* Get number of classifications in table */
        int num_classes = lua_rawlen(L, index);
        if(num_classes > 0 && provided) *provided = true;

        /* Iterate through each classification in table */
        for(int i = 0; i < num_classes; i++)
        {
            /* Get classification */
            lua_rawgeti(L, index, i+1);

            /* Set classification */
            if(lua_isinteger(L, -1))
            {
                int classification = LuaObject::getLuaInteger(L, -1);
                if(classification >= 0 && classification < NUM_ATL08_CLASSES)
                {
                    parms->atl08_class[classification] = true;
                    mlog(DEBUG, "Selecting classification %d", classification);
                }
                else
                {
                    mlog(ERROR, "Invalid ATL08 classification: %d", classification);
                }
            }
            else if(lua_isstring(L, -1))
            {
                const char* classifiction_str = LuaObject::getLuaString(L, -1);
                atl08_classification_t atl08class = str2atl08class(classifiction_str);
                if(atl08class != ATL08_INVALID_CLASSIFICATION)
                {
                    parms->atl08_class[atl08class] = true;
                    mlog(DEBUG, "Selecting %s classification", classifiction_str);
                }
                else
                {
                    mlog(ERROR, "Invalid ATL08 classification: %s", classifiction_str);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(lua_isinteger(L, index))
    {
        /* Clear classification table (sets all to false) */
        LocalLib::set(parms->atl08_class, 0, sizeof(parms->atl08_class));

        /* Set classification */
        int classification = LuaObject::getLuaInteger(L, -1);
        if(classification >= 0 && classification < NUM_ATL08_CLASSES)
        {
            parms->atl08_class[classification] = true;
            mlog(DEBUG, "Selecting classification %d", classification);
        }
        else
        {
            mlog(ERROR, "Invalid ATL08 classification: %d", classification);
        }
    }
    else if(lua_isstring(L, index))
    {
        /* Clear classification table (sets all to false) */
        LocalLib::set(parms->atl08_class, 0, sizeof(parms->atl08_class));

        /* Set classification */
        const char* classifiction_str = LuaObject::getLuaString(L, index);
        atl08_classification_t atl08class = str2atl08class(classifiction_str);
        if(atl08class != ATL08_INVALID_CLASSIFICATION)
        {
            if(provided) *provided = true;
            parms->atl08_class[atl08class] = true;
            mlog(DEBUG, "Selecting %s classification", classifiction_str);
        }
        else
        {
            mlog(ERROR, "Invalid ATL08 classification: %s", classifiction_str);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "ATL08 classification must be provided as a table or string");
    }
}

/*----------------------------------------------------------------------------
 * get_lua_polygon
 *----------------------------------------------------------------------------*/
static void get_lua_polygon (lua_State* L, int index, icesat2_parms_t* parms, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        /* Get Number of Points in Polygon */
        int num_points = lua_rawlen(L, index);

        /* Iterate through each coordinate */
        for(int i = 0; i < num_points; i++)
        {
            /* Get coordinate table */
            lua_rawgeti(L, index, i+1);
            if(lua_istable(L, index))
            {
                MathLib::coord_t coord;

                /* Get longitude entry */
                lua_getfield(L, index, LUA_PARM_LONGITUDE);
                coord.lon = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Get latitude entry */
                lua_getfield(L, index, LUA_PARM_LATITUDE);
                coord.lat = LuaObject::getLuaFloat(L, -1);
                lua_pop(L, 1);

                /* Add Coordinate */
                parms->polygon.add(coord);
                if(provided) *provided = true;
            }
            lua_pop(L, 1);
        }
    }
}

/*----------------------------------------------------------------------------
 * get_lua_raster
 *----------------------------------------------------------------------------*/
static void get_lua_raster (lua_State* L, int index, icesat2_parms_t* parms, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        try
        {
            parms->raster = GeoJsonRaster::create(L, index);
            *provided = true;
        }
        catch(const RunTimeException& e)
        {
            mlog(e.level(), "Error creating GeoJsonRaster file: %s", e.what());
        }
    }
}

/*----------------------------------------------------------------------------
 * get_lua_yapc
 *----------------------------------------------------------------------------*/
static void get_lua_yapc (lua_State* L, int index, icesat2_parms_t* parms, bool* provided)
{
    bool field_provided;

    /* Reset Provided */
    *provided = false;

    /* Must be table of coordinates */
    if(lua_istable(L, index))
    {
        *provided = true;

        lua_getfield(L, index, LUA_PARM_YAPC_SCORE);
        parms->yapc.score = (uint8_t)LuaObject::getLuaInteger(L, -1, true, parms->yapc.score, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_YAPC_SCORE, (int)parms->yapc.score);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_YAPC_VERSION);
        parms->yapc.version = (int)LuaObject::getLuaInteger(L, -1, true, parms->yapc.version, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_YAPC_VERSION, (int)parms->yapc.version);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_YAPC_KNN);
        parms->yapc.knn = (int)LuaObject::getLuaInteger(L, -1, true, parms->yapc.knn, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_YAPC_KNN, (int)parms->yapc.knn);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_YAPC_MIN_KNN);
        parms->yapc.min_knn = (int)LuaObject::getLuaInteger(L, -1, true, parms->yapc.min_knn, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_YAPC_MIN_KNN, (int)parms->yapc.min_knn);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_YAPC_WIN_H);
        parms->yapc.win_h = LuaObject::getLuaFloat(L, -1, true, parms->yapc.win_h, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %.3lf", LUA_PARM_YAPC_WIN_H, parms->yapc.win_h);
        lua_pop(L, 1);

        lua_getfield(L, index, LUA_PARM_YAPC_WIN_X);
        parms->yapc.win_x = LuaObject::getLuaFloat(L, -1, true, parms->yapc.win_x, &field_provided);
        if(field_provided) mlog(DEBUG, "Setting %s to %.3lf", LUA_PARM_YAPC_WIN_X, parms->yapc.win_x);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * get_lua_field_list
 *----------------------------------------------------------------------------*/
static void get_lua_field_list (lua_State* L, int index, ancillary_list_t** field_list, bool* provided)
{
    /* Reset Provided */
    *provided = false;

    /* Must be table of fields specified as strings */
    if(lua_istable(L, index))
    {
        /* Allocate Field List */
        *field_list = new ancillary_list_t;
        *provided = true;

        /* Get number of fields in table */
        int num_fields = lua_rawlen(L, index);
        if(num_fields > 0 && provided) *provided = true;

        /* Iterate through each fields in table */
        for(int i = 0; i < num_fields; i++)
        {
            /* Get classification */
            lua_rawgeti(L, index, i+1);

            if(lua_isstring(L, -1))
            {
                const char* field_str = LuaObject::getLuaString(L, -1);
                SafeString field("%s", field_str);
                (*field_list)->add(field);
                mlog(DEBUG, "Adding %s to list of ancillary fields", field_str);
            }
            else
            {
                mlog(ERROR, "Invalid field specified - must be a string");
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "Field lists must be provided as a table");
    }
}

/******************************************************************************
 * EXPORTED FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * getLuaIcesat2Parms
 *----------------------------------------------------------------------------*/
icesat2_parms_t* getLuaIcesat2Parms (lua_State* L, int index)
{
    icesat2_parms_t* parms = new icesat2_parms_t; // freed in ATL03Reader and ATL06Dispatch destructor
    *parms = DefaultParms; // initialize with defaults

    if(lua_type(L, index) == LUA_TTABLE)
    {
        try
        {
            bool provided = false;

            lua_getfield(L, index, LUA_PARM_SURFACE_TYPE);
            parms->surface_type = (surface_type_t)LuaObject::getLuaInteger(L, -1, true, parms->surface_type, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_SURFACE_TYPE, (int)parms->surface_type);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ATL03_CNF);
            get_lua_atl03_cnf(L, -1, parms, &provided);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_QUALITY);
            get_lua_atl03_quality(L, -1, parms, &provided);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_YAPC);
            get_lua_yapc(L, -1, parms, &provided);
            if(provided) parms->stages[STAGE_YAPC] = true;
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_PASS_INVALID);
            parms->pass_invalid = LuaObject::getLuaBoolean(L, -1, true, parms->pass_invalid, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %s", LUA_PARM_PASS_INVALID, parms->pass_invalid ? "true" : "false");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_DISTANCE_IN_SEGMENTS);
            parms->dist_in_seg = LuaObject::getLuaBoolean(L, -1, true, parms->dist_in_seg, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %s", LUA_PARM_DISTANCE_IN_SEGMENTS, parms->dist_in_seg ? "true" : "false");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ATL08_CLASS);
            get_lua_atl08_class(L, -1, parms, &provided);
            if(provided) parms->stages[STAGE_ATL08] = true;
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_POLYGON);
            get_lua_polygon(L, -1, parms, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %d points", LUA_PARM_POLYGON, (int)parms->polygon.length());
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_RASTER);
            get_lua_raster(L, -1, parms, &provided);
            if(provided) mlog(DEBUG, "Setting %s file for use", LUA_PARM_RASTER);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_TRACK);
            parms->track = LuaObject::getLuaInteger(L, -1, true, parms->track, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_TRACK, parms->track);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_COMPACT);
            parms->compact = LuaObject::getLuaBoolean(L, -1, true, parms->compact, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %s", LUA_PARM_COMPACT, parms->compact ? "true" : "false");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MAX_ITERATIONS);
            parms->max_iterations = LuaObject::getLuaInteger(L, -1, true, parms->max_iterations, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_MAX_ITERATIONS, (int)parms->max_iterations);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ALONG_TRACK_SPREAD);
            parms->along_track_spread = LuaObject::getLuaFloat(L, -1, true, parms->along_track_spread, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", LUA_PARM_ALONG_TRACK_SPREAD, parms->along_track_spread);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MIN_PHOTON_COUNT);
            parms->minimum_photon_count = LuaObject::getLuaInteger(L, -1, true, parms->minimum_photon_count, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %d", LUA_PARM_MIN_PHOTON_COUNT, parms->minimum_photon_count);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MIN_WINDOW);
            parms->minimum_window = LuaObject::getLuaFloat(L, -1, true, parms->minimum_window, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", LUA_PARM_MIN_WINDOW, parms->minimum_window);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_MAX_ROBUST_DISPERSION);
            parms->maximum_robust_dispersion = LuaObject::getLuaFloat(L, -1, true, parms->maximum_robust_dispersion, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", LUA_PARM_MAX_ROBUST_DISPERSION, parms->maximum_robust_dispersion);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_EXTENT_LENGTH);
            parms->extent_length = LuaObject::getLuaFloat(L, -1, true, parms->extent_length, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", LUA_PARM_EXTENT_LENGTH, parms->extent_length);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_EXTENT_STEP);
            parms->extent_step = LuaObject::getLuaFloat(L, -1, true, parms->extent_step, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", LUA_PARM_EXTENT_STEP, parms->extent_step);
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ATL03_GEOLOCATION_FIELDS);
            get_lua_field_list (L, -1, &parms->atl03_geolocation_fields, &provided);
            if(provided) mlog(DEBUG, "ATL03 geolocation field array detected");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ATL03_GEOCORRECTION_FIELDS);
            get_lua_field_list (L, -1, &parms->atl03_geocorrection_fields, &provided);
            if(provided) mlog(DEBUG, "ATL03 geocorrection field array detected");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ATL03_HEIGHT_FIELDS);
            get_lua_field_list (L, -1, &parms->atl03_height_fields, &provided);
            if(provided) mlog(DEBUG, "ATL03 height field array detected");
            lua_pop(L, 1);

            lua_getfield(L, index, LUA_PARM_ATL08_SIGNAL_PHOTON_FIELDS);
            get_lua_field_list (L, -1, &parms->atl08_signal_photon_fields, &provided);
            if(provided) mlog(DEBUG, "ATL08 signal photon field array detected");
            lua_pop(L, 1);
        }
        catch(const RunTimeException& e)
        {
            delete parms; // free allocated parms since it won't be owned by anything else
            throw; // rethrow exception
        }
    }

    return parms;
}
