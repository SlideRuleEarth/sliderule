/*
 * Copyright (c) 2023, University of Texas
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
 * 3. Neither the name of the University of Texas nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF TEXAS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF TEXAS OR
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
#include "geo.h"
#include "BathyParms.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* BathyParms::MAX_DEM_DELTA = "max_dem_delta";
const char* BathyParms::PH_IN_EXTENT = "ph_in_extent";
const char* BathyParms::GENERATE_NDWI = "generate_ndwi";
const char* BathyParms::USE_BATHY_MASK = "use_bathy_mask";
const char* BathyParms::RETURN_INPUTS = "return_inputs";
const char* BathyParms::CLASSIFIERS = "classifiers";
const char* BathyParms::SPOTS = "spots";
const char* BathyParms::ATL09_RESOURCES = "resources09";
const char* BathyParms::SURFACE_FINDER = "surface_finder";
const char* BathyParms::DEM_BUFFER = "dem_buffer";
const char* BathyParms::BIN_SIZE = "bin_size";
const char* BathyParms::MAX_RANGE = "max_range";
const char* BathyParms::MAX_BINS = "max_bins";
const char* BathyParms::SIGNAL_THRESHOLD_SIGMAS = "signal_threshold_sigmas";
const char* BathyParms::MIN_PEAK_SEPARATION = "min_peak_separation";
const char* BathyParms::HIGHEST_PEAK_RATIO = "highest_peak_ratio";
const char* BathyParms::SURFACE_WIDTH_SIGMAS = "surface_width_sigmas";
const char* BathyParms::MODEL_AS_POISSON = "model_as_poisson";
const char* BathyParms::OUTPUT_AS_SDP = "output_as_sdp";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int BathyParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Requests parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new BathyParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * getATL09Key
 *----------------------------------------------------------------------------*/
void BathyParms::getATL09Key (char* key, const char* name)
{
    // Example
    //  Name: ATL09_20230601012940_10951901_006_01.h5
    //  Key:                       10951901
    if(StringLib::size(name) != ATL09_RESOURCE_NAME_LEN)
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to process ATL09 resource name: %s", name);
    }
    StringLib::copy(key, &name[21], ATL09_RESOURCE_KEY_LEN + 1);
    key[ATL09_RESOURCE_KEY_LEN] = '\0';
}

/*----------------------------------------------------------------------------
 * luaSpotEnabled - :spoton(<spot>) --> true|false
 *----------------------------------------------------------------------------*/
int BathyParms::luaSpotEnabled (lua_State* L)
{
    bool status = false;
    BathyParms* lua_obj = NULL;

    try
    {
        lua_obj = dynamic_cast<BathyParms*>(getLuaSelf(L, 1));
        const int spot = getLuaInteger(L, 2);
        if(spot >= 1 && spot <= NUM_SPOTS)
        {
            status = lua_obj->spots[spot-1];
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error retrieving spot status: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * luaClassifierEnabled - :classifieron(<spot>) --> true|false
 *----------------------------------------------------------------------------*/
int BathyParms::luaClassifierEnabled (lua_State* L)
{
    bool status = false;
    BathyParms* lua_obj = NULL;

    try
    {
        lua_obj = dynamic_cast<BathyParms*>(getLuaSelf(L, 1));
        const char* classifier_str = getLuaString(L, 2);
        const classifier_t classifier = str2classifier(classifier_str);
        if(classifier != INVALID_CLASSIFIER)
        {
            const int index = static_cast<int>(classifier);
            status = lua_obj->classifiers[index];
        }
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error retrieving classifier status: %s", e.what());
    }

    lua_pushboolean(L, status);
    return 1;
}

/*----------------------------------------------------------------------------
 * str2classifier
 *----------------------------------------------------------------------------*/
BathyParms::classifier_t BathyParms::str2classifier (const char* str)
{
    if(StringLib::match(str, "qtrees"))             return QTREES;
    if(StringLib::match(str, "coastnet"))           return COASTNET;
    if(StringLib::match(str, "openoceans"))         return OPENOCEANS;
    if(StringLib::match(str, "medianfilter"))       return MEDIANFILTER;
    if(StringLib::match(str, "cshelph"))            return CSHELPH;
    if(StringLib::match(str, "bathypathfinder"))    return BATHY_PATHFINDER;
    if(StringLib::match(str, "pointnet2"))          return POINTNET2;
    if(StringLib::match(str, "localcontrast"))      return LOCAL_CONTRAST;
    if(StringLib::match(str, "ensemble"))           return ENSEMBLE;
    return INVALID_CLASSIFIER;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyParms::BathyParms(lua_State* L, int index):
    Icesat2Parms (L, index),
    max_dem_delta (10000.0),
    ph_in_extent (8192),
    generate_ndwi (true),
    use_bathy_mask (true),
    classifiers {true, true, true, true, true, true, true, true, true},
    return_inputs (false),
    spots {true, true, true, true, true, true},
    output_as_sdp (false),
    surface_finder {
        .dem_buffer = 50.0,
        .bin_size = 0.5,
        .max_range = 1000.0,
        .max_bins = 10000,
        .signal_threshold_sigmas = 3.0,
        .min_peak_separation = 0.5,
        .highest_peak_ratio = 1.2,
        .surface_width_sigmas = 3.0,
        .model_as_poisson = true
    }
{
    bool provided = false;

    /* Set Meta Table Functions */
    luaL_getmetatable(L, LUA_META_NAME);
    LuaEngine::setAttrFunc(L, "spoton", luaSpotEnabled);
    LuaEngine::setAttrFunc(L, "classifieron", luaClassifierEnabled);
    lua_pop(L, 1);

    try
    {
        /* maximum DEM delta */
        lua_getfield(L, index, BathyParms::MAX_DEM_DELTA);
        max_dem_delta = LuaObject::getLuaFloat(L, -1, true, max_dem_delta, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::MAX_DEM_DELTA, max_dem_delta);
        lua_pop(L, 1);

        /* photons in extent */
        lua_getfield(L, index, BathyParms::PH_IN_EXTENT);
        ph_in_extent = LuaObject::getLuaInteger(L, -1, true, ph_in_extent, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::PH_IN_EXTENT, ph_in_extent);
        lua_pop(L, 1);

        /* generate ndwi */
        lua_getfield(L, index, BathyParms::GENERATE_NDWI);
        generate_ndwi = LuaObject::getLuaBoolean(L, -1, true, generate_ndwi, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::GENERATE_NDWI, generate_ndwi);
        lua_pop(L, 1);

        /* use bathy mask */
        lua_getfield(L, index, BathyParms::USE_BATHY_MASK);
        use_bathy_mask = LuaObject::getLuaBoolean(L, -1, true, use_bathy_mask, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::USE_BATHY_MASK, use_bathy_mask);
        lua_pop(L, 1);

        /* classifiers */
        lua_getfield(L, index, BathyParms::CLASSIFIERS);
        get_classifiers(L, -1, &provided);
        lua_pop(L, 1);

        /* return inputs */
        lua_getfield(L, index, BathyParms::RETURN_INPUTS);
        return_inputs = LuaObject::getLuaBoolean(L, -1, true, return_inputs, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::RETURN_INPUTS, return_inputs);
        lua_pop(L, 1);

        /* output as sdp */
        lua_getfield(L, index, BathyParms::OUTPUT_AS_SDP);
        output_as_sdp = LuaObject::getLuaBoolean(L, -1, true, output_as_sdp, &provided);
        if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::OUTPUT_AS_SDP, output_as_sdp);
        lua_pop(L, 1);

        /* atl09 resources */
        lua_getfield(L, index, BathyParms::ATL09_RESOURCES);
        get_atl09_list(L, -1, &provided);
        lua_pop(L, 1);

        /* spot selection */
        lua_getfield(L, index, BathyParms::SPOTS);
        get_spot_list(L, -1, &provided);
        lua_pop(L, 1);

        /* surface finder */
        lua_getfield(L, index, BathyParms::SURFACE_FINDER);
        if(lua_istable(L, index))
        {
            /* dem buffer */
            lua_getfield(L, index, BathyParms::DEM_BUFFER);
            surface_finder.dem_buffer = LuaObject::getLuaFloat(L, -1, true, surface_finder.dem_buffer, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::DEM_BUFFER, surface_finder.dem_buffer);
            lua_pop(L, 1);

            /* bin size */
            lua_getfield(L, index, BathyParms::BIN_SIZE);
            surface_finder.bin_size = LuaObject::getLuaFloat(L, -1, true, surface_finder.bin_size, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::BIN_SIZE, surface_finder.bin_size);
            lua_pop(L, 1);

            /* max range */
            lua_getfield(L, index, BathyParms::MAX_RANGE);
            surface_finder.max_range = LuaObject::getLuaFloat(L, -1, true, surface_finder.max_range, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::MAX_RANGE, surface_finder.max_range);
            lua_pop(L, 1);

            /* max bins */
            lua_getfield(L, index, BathyParms::MAX_BINS);
            surface_finder.max_bins = LuaObject::getLuaInteger(L, -1, true, surface_finder.max_bins, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %ld", BathyParms::MAX_BINS, surface_finder.max_bins);
            lua_pop(L, 1);

            /* signal threshold sigmas */
            lua_getfield(L, index, BathyParms::SIGNAL_THRESHOLD_SIGMAS);
            surface_finder.signal_threshold_sigmas = LuaObject::getLuaFloat(L, -1, true, surface_finder.signal_threshold_sigmas, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::SIGNAL_THRESHOLD_SIGMAS, surface_finder.signal_threshold_sigmas);
            lua_pop(L, 1);

            /* min peak separation */
            lua_getfield(L, index, BathyParms::MIN_PEAK_SEPARATION);
            surface_finder.min_peak_separation = LuaObject::getLuaFloat(L, -1, true, surface_finder.min_peak_separation, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::MIN_PEAK_SEPARATION, surface_finder.min_peak_separation);
            lua_pop(L, 1);

            /* highest peak ratio */
            lua_getfield(L, index, BathyParms::HIGHEST_PEAK_RATIO);
            surface_finder.highest_peak_ratio = LuaObject::getLuaFloat(L, -1, true, surface_finder.highest_peak_ratio, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::HIGHEST_PEAK_RATIO, surface_finder.highest_peak_ratio);
            lua_pop(L, 1);

            /* surface width sigmas */
            lua_getfield(L, index, BathyParms::SURFACE_WIDTH_SIGMAS);
            surface_finder.surface_width_sigmas = LuaObject::getLuaFloat(L, -1, true, surface_finder.surface_width_sigmas, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %lf", BathyParms::SURFACE_WIDTH_SIGMAS, surface_finder.surface_width_sigmas);
            lua_pop(L, 1);

            /* model as poisson */
            lua_getfield(L, index, BathyParms::MODEL_AS_POISSON);
            surface_finder.model_as_poisson = LuaObject::getLuaBoolean(L, -1, true, surface_finder.model_as_poisson, &provided);
            if(provided) mlog(DEBUG, "Setting %s to %d", BathyParms::MODEL_AS_POISSON, surface_finder.model_as_poisson);
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw; // rethrow exception
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyParms::~BathyParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void BathyParms::cleanup (void) const
{
}

/*----------------------------------------------------------------------------
 * get_atl09_list
 *----------------------------------------------------------------------------*/
void BathyParms::get_atl09_list (lua_State* L, int index, bool* provided)
{
    /* Reset provided */
    if(provided) *provided = false;

    /* Must be table of strings */
    if(lua_istable(L, index))
    {
        /* Get number of item in table */
        const int num_strings = lua_rawlen(L, index);
        if(num_strings > 0 && provided) *provided = true;

        /* Iterate through each item in table */
        for(int i = 0; i < num_strings; i++)
        {
            /* Get item */
            lua_rawgeti(L, index, i+1);
            if(lua_isstring(L, -1))
            {
                // Example
                //  Name: ATL09_20230601012940_10951901_006_01.h5
                //  Key:                       10951901
                const char* str = LuaObject::getLuaString(L, -1);
                char key[ATL09_RESOURCE_KEY_LEN + 1];
                getATL09Key(key, str); // throws on error
                const string name(str);
                if(!alt09_index.add(key, name, true))
                {
                    mlog(CRITICAL, "Duplicate ATL09 key detected: %s", str);
                }
                mlog(DEBUG, "Adding %s to ATL09 index with key: %s", str, key);
            }
            else
            {
                mlog(ERROR, "Invalid ATL09 item specified - must be a string");
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "ATL09 lists must be provided as a table");
    }
}

/*----------------------------------------------------------------------------
 * get_spot_list
 *----------------------------------------------------------------------------*/
void BathyParms::get_spot_list (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of spots or a single spot */
    if(lua_istable(L, index))
    {
        /* Clear spot table (sets all to false) */
        memset(spots, 0, sizeof(spots));
        if(provided) *provided = true;

        /* Iterate through each spot in table */
        const int num_spots = lua_rawlen(L, index);
        for(int i = 0; i < num_spots; i++)
        {
            /* Get spot */
            lua_rawgeti(L, index, i+1);

            /* Set spot */
            if(lua_isinteger(L, -1))
            {
                const int spot = LuaObject::getLuaInteger(L, -1);
                if(spot >= 1 && spot <= NUM_SPOTS)
                {
                    spots[spot-1] = true;
                    mlog(DEBUG, "Selecting spot %d", spot);
                }
                else
                {
                    mlog(ERROR, "Invalid spot: %d", spot);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(lua_isinteger(L, index))
    {
        /* Clear spot table (sets all to false) */
        memset(spots, 0, sizeof(spots));

        /* Set spot */
        const int spot = LuaObject::getLuaInteger(L, -1);
        if(spot >= 1 && spot <= NUM_SPOTS)
        {
            spots[spot-1] = true;
            mlog(DEBUG, "Selecting spot %d", spot);
        }
        else
        {
            mlog(ERROR, "Invalid spot: %d", spot);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "Spot selection must be provided as a table or integer");
    }
}

/*----------------------------------------------------------------------------
 * get_classifiers
 *----------------------------------------------------------------------------*/
void BathyParms::get_classifiers (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of classifiers or a single classifier as a string */
    if(lua_istable(L, index))
    {
        /* Clear classifier table (sets all to false) */
        memset(classifiers, 0, sizeof(classifiers));

        /* Get number of classifiers in table */
        const int num_classifiers = lua_rawlen(L, index);
        if(num_classifiers > 0 && provided) *provided = true;

        /* Iterate through each classifier in table */
        for(int i = 0; i < num_classifiers; i++)
        {
            /* Get classifier */
            lua_rawgeti(L, index, i+1);

            /* Set classifier */
            if(lua_isinteger(L, -1))
            {
                const int classifier = LuaObject::getLuaInteger(L, -1);
                if(classifier >= 0 && classifier < NUM_CLASSIFIERS)
                {
                    classifiers[classifier] = true;
                    mlog(DEBUG, "Selecting classifier %d", classifier);
                }
                else
                {
                    mlog(ERROR, "Invalid classifier: %d", classifier);
                }
            }
            else if(lua_isstring(L, -1))
            {
                const char* classifier_str = LuaObject::getLuaString(L, -1);
                const classifier_t classifier = str2classifier(classifier_str);
                if(classifier != INVALID_CLASSIFIER)
                {
                    classifiers[static_cast<int>(classifier)] = true;
                    mlog(DEBUG, "Selecting %s classifier", classifier_str);
                }
                else
                {
                    mlog(ERROR, "Invalid classifier: %s", classifier_str);
                }
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(lua_isinteger(L, index))
    {
        /* Clear classifier table (sets all to false) */
        memset(classifiers, 0, sizeof(classifiers));

        /* Set classifier */
        const int classifier = LuaObject::getLuaInteger(L, -1);
        if(classifier >= 0 && classifier < NUM_CLASSIFIERS)
        {
            if(provided) *provided = true;
            classifiers[classifier] = true;
            mlog(DEBUG, "Selecting classifier %d", classifier);
        }
        else
        {
            mlog(ERROR, "Invalid classifier: %d", classifier);
        }
    }
    else if(lua_isstring(L, index))
    {
        /* Clear classifiers table (sets all to false) */
        memset(classifiers, 0, sizeof(classifiers));

        /* Set classifier */
        const char* classifier_str = LuaObject::getLuaString(L, index);
        const classifier_t classifier = str2classifier(classifier_str);
        if(classifier != INVALID_CLASSIFIER)
        {
            if(provided) *provided = true;
            classifiers[static_cast<int>(classifier)] = true;
            mlog(DEBUG, "Selecting %s classifier", classifier_str);
        }
        else
        {
            mlog(ERROR, "Invalid classifier: %s", classifier_str);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "ATL24 classifiers must be provided as a table, integer, or string");
    }
}
