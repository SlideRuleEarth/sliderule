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
#include "geo.h"
#include "BathyParms.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

/* Bathy Parameters */
static const char* BATHY_PARMS                      = "bathy";
static const char* BATHY_PARMS_READER               = "reader";
static const char* BATHY_PARMS_REFRACTION           = "refraction";
static const char* BATHY_PARMS_UNCERTAINTY          = "uncertainty";

/* Reader Parameters */
static const char* BATHY_PARMS_ASSET                = "asset";
static const char* BATHY_PARMS_ASSET09              = "asset09";
static const char* BATHY_PARMS_DEFAULT_ASSET09      = "icesat2";
static const char* BATHY_PARMS_ATL03_RESOURCE       = "resource";
static const char* BATHY_PARMS_ATL09_RESOURCE       = "resource09";
static const char* BATHY_PARMS_MAX_DEM_DELTA        = "max_dem_delta";
static const char* BATHY_PARMS_MIN_DEM_DELTA        = "min_dem_delta";
static const char* BATHY_PARMS_PH_IN_EXTENT         = "ph_in_extent";
static const char* BATHY_PARMS_GENERATE_NDWI        = "generate_ndwi";
static const char* BATHY_PARMS_USE_BATHY_MASK       = "use_bathy_mask";
static const char* BATHY_PARMS_CLASSIFIERS          = "classifiers";
static const char* BATHY_PARMS_RETURN_INPUTS        = "return_inputs";
static const char* BATHY_PARMS_OUTPUT_AS_SDP        = "output_as_sdp";
static const char* BATHY_PARMS_BIN_SIZE             = "bin_size";
static const char* BATHY_PARMS_MAX_RANGE            = "max_range";
static const char* BATHY_PARMS_MAX_BINS             = "max_bins";
static const char* BATHY_PARMS_SIGNAL_THRESHOLD     = "signal_threshold"; // sigmas
static const char* BATHY_PARMS_MIN_PEAK_SEPARATION  = "min_peak_separation";
static const char* BATHY_PARMS_HIGHEST_PEAK_RATIO   = "highest_peak_ratio";
static const char* BATHY_PARMS_SURFACE_WIDTH        = "surface_width"; // sigmas
static const char* BATHY_PARMS_MODEL_AS_POISSON     = "model_as_poisson"; // sigmas
static const char* BATHY_PARMS_SPOTS                = "spots";
static const char* BATHY_PARMS_DEFAULT_ASSET        = "icesat2";

/* Refraction Parameters */
static const char* BATHY_PARMS_RI_AIR               = "ri_air";
static const char* BATHY_PARMS_RI_WATER             = "ri_water";

/* Uncertainty Parameters */
static const char* BATHY_PARMS_ASSET_KD             = "asset_kd";
static const char* BATHY_PARMS_DEFAULT_ASSETKD      = "viirsj1-s3";
static const char* BATHY_PARMS_RESOURCE_KD          = "resource_kd";

/* Photon Record Definition */
const char* BathyParms::phRecType = "bathyrec.photons";
const RecordObject::fieldDef_t BathyParms::phRecDef[] = {
    {"time",            RecordObject::TIME8,    offsetof(photon_t, time_ns),        1,  NULL, NATIVE_FLAGS | RecordObject::TIME},
    {"index_ph",        RecordObject::INT32,    offsetof(photon_t, index_ph),       1,  NULL, NATIVE_FLAGS | RecordObject::INDEX},
    {"index_seg",       RecordObject::INT32,    offsetof(photon_t, index_seg),      1,  NULL, NATIVE_FLAGS},
    {"lat_ph",          RecordObject::DOUBLE,   offsetof(photon_t, lat_ph),         1,  NULL, NATIVE_FLAGS | RecordObject::Y_COORD},
    {"lon_ph",          RecordObject::DOUBLE,   offsetof(photon_t, lon_ph),         1,  NULL, NATIVE_FLAGS | RecordObject::X_COORD},
    {"x_ph",            RecordObject::DOUBLE,   offsetof(photon_t, x_ph),           1,  NULL, NATIVE_FLAGS},
    {"y_ph",            RecordObject::DOUBLE,   offsetof(photon_t, y_ph),           1,  NULL, NATIVE_FLAGS},
    {"x_atc",           RecordObject::DOUBLE,   offsetof(photon_t, x_atc),          1,  NULL, NATIVE_FLAGS},
    {"y_atc",           RecordObject::DOUBLE,   offsetof(photon_t, y_atc),          1,  NULL, NATIVE_FLAGS},
    {"background_rate", RecordObject::DOUBLE,   offsetof(photon_t, background_rate),1,  NULL, NATIVE_FLAGS},
    {"ellipse_h",       RecordObject::FLOAT,    offsetof(photon_t, ellipse_h),      1,  NULL, NATIVE_FLAGS},
    {"ortho_h",         RecordObject::FLOAT,    offsetof(photon_t, ortho_h),        1,  NULL, NATIVE_FLAGS | RecordObject::Z_COORD},
    {"surface_h",       RecordObject::FLOAT,    offsetof(photon_t, surface_h),      1,  NULL, NATIVE_FLAGS},
    {"yapc_score",      RecordObject::UINT8,    offsetof(photon_t, yapc_score),     1,  NULL, NATIVE_FLAGS},
    {"max_signal_conf", RecordObject::INT8,     offsetof(photon_t, max_signal_conf),1,  NULL, NATIVE_FLAGS},
    {"quality_ph",      RecordObject::INT8,     offsetof(photon_t, quality_ph),     1,  NULL, NATIVE_FLAGS},
};

/* Extent Record Definition */
const char* BathyParms::exRecType = "bathyrec";
const RecordObject::fieldDef_t BathyParms::exRecDef[] = {
    {"region",          RecordObject::UINT8,    offsetof(extent_t, region),                 1,  NULL, NATIVE_FLAGS},
    {"track",           RecordObject::UINT8,    offsetof(extent_t, track),                  1,  NULL, NATIVE_FLAGS},
    {"pair",            RecordObject::UINT8,    offsetof(extent_t, pair),                   1,  NULL, NATIVE_FLAGS},
    {"spot",            RecordObject::UINT8,    offsetof(extent_t, spot),                   1,  NULL, NATIVE_FLAGS},
    {"rgt",             RecordObject::UINT16,   offsetof(extent_t, reference_ground_track), 1,  NULL, NATIVE_FLAGS},
    {"cycle",           RecordObject::UINT8,    offsetof(extent_t, cycle),                  1,  NULL, NATIVE_FLAGS},
    {"utm_zone",        RecordObject::UINT8,    offsetof(extent_t, utm_zone),               1,  NULL, NATIVE_FLAGS},
    {"extent_id",       RecordObject::UINT64,   offsetof(extent_t, extent_id),              1,  NULL, NATIVE_FLAGS},
    {"wind_v",          RecordObject::FLOAT,    offsetof(extent_t, wind_v),                 1,  NULL, NATIVE_FLAGS},
    {"ndwi",            RecordObject::FLOAT,    offsetof(extent_t, ndwi),                   1,  NULL, NATIVE_FLAGS},
    {"photons",         RecordObject::USER,     offsetof(extent_t, photons),                0,  phRecType, NATIVE_FLAGS | RecordObject::BATCH} // variable length
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * reader_t::fromLua
 *----------------------------------------------------------------------------*/
void BathyParms::reader_t::fromLua (lua_State* L, int index)
{
    /* Get Algorithm Parameters */
    if(lua_istable(L, index))
    {
        /* asset */
        lua_getfield(L, index, BATHY_PARMS_ASSET);
        const char* asset_name = LuaObject::getLuaString(L, -1, true, BATHY_PARMS_DEFAULT_ASSET);
        asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE));
        if(!asset) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to find asset %s", asset_name);
        lua_pop(L, 1);

        /* asset09 */
        lua_getfield(L, index, BATHY_PARMS_ASSET09);
        const char* asset09_name = LuaObject::getLuaString(L, -1, true, BATHY_PARMS_DEFAULT_ASSET09);
        asset09 = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset09_name, Asset::OBJECT_TYPE));
        if(!asset09) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to find asset %s", asset09_name);
        lua_pop(L, 1);

        /* atl03 resources */
        lua_getfield(L, index, BATHY_PARMS_ATL03_RESOURCE);
        resource = StringLib::duplicate(LuaObject::getLuaString(L, -1));
        lua_pop(L, 1);

        /* atl09 resources */
        lua_getfield(L, index, BATHY_PARMS_ATL09_RESOURCE);
        resource09 = StringLib::duplicate(LuaObject::getLuaString(L, -1));
        lua_pop(L, 1);

        /* maximum DEM delta */
        lua_getfield(L, index, BATHY_PARMS_MAX_DEM_DELTA);
        max_dem_delta = LuaObject::getLuaFloat(L, -1, true, max_dem_delta, NULL);
        lua_pop(L, 1);

        /* minimum DEM delta */
        lua_getfield(L, index, BATHY_PARMS_MIN_DEM_DELTA);
        min_dem_delta = LuaObject::getLuaFloat(L, -1, true, min_dem_delta, NULL);
        lua_pop(L, 1);

        /* photons in extent */
        lua_getfield(L, index, BATHY_PARMS_PH_IN_EXTENT);
        ph_in_extent = LuaObject::getLuaInteger(L, -1, true, ph_in_extent, NULL);
        lua_pop(L, 1);

        /* generate ndwi */
        lua_getfield(L, index, BATHY_PARMS_GENERATE_NDWI);
        generate_ndwi = LuaObject::getLuaBoolean(L, -1, true, generate_ndwi, NULL);
        lua_pop(L, 1);

        /* use bathy mask */
        lua_getfield(L, index, BATHY_PARMS_USE_BATHY_MASK);
        use_bathy_mask = LuaObject::getLuaBoolean(L, -1, true, use_bathy_mask, NULL);
        lua_pop(L, 1);

        /* classifiers */
        lua_getfield(L, index, BATHY_PARMS_CLASSIFIERS);
        getClassifiers(L, -1, NULL, classifiers);
        lua_pop(L, 1);

        /* return inputs */
        lua_getfield(L, index, BATHY_PARMS_RETURN_INPUTS);
        return_inputs = LuaObject::getLuaBoolean(L, -1, true, return_inputs, NULL);
        lua_pop(L, 1);

        /* output as sdp */
        lua_getfield(L, index, BATHY_PARMS_OUTPUT_AS_SDP);
        output_as_sdp = LuaObject::getLuaBoolean(L, -1, true, output_as_sdp, NULL);
        lua_pop(L, 1);

        /* spot selection */
        lua_getfield(L, index, BATHY_PARMS_SPOTS);
        getSpotList(L, -1, NULL, spots);
        lua_pop(L, 1);

        /* bin size */
        lua_getfield(L, index, BATHY_PARMS_BIN_SIZE);
        bin_size = LuaObject::getLuaFloat(L, -1, true, bin_size, NULL);
        lua_pop(L, 1);

        /* max range */
        lua_getfield(L, index, BATHY_PARMS_MAX_RANGE);
        max_range = LuaObject::getLuaFloat(L, -1, true, max_range, NULL);
        lua_pop(L, 1);

        /* max bins */
        lua_getfield(L, index, BATHY_PARMS_MAX_BINS);
        max_bins = LuaObject::getLuaInteger(L, -1, true, max_bins, NULL);
        lua_pop(L, 1);

        /* signal threshold */
        lua_getfield(L, index, BATHY_PARMS_SIGNAL_THRESHOLD);
        signal_threshold = LuaObject::getLuaFloat(L, -1, true, signal_threshold, NULL);
        lua_pop(L, 1);

        /* minimum peak separation */
        lua_getfield(L, index, BATHY_PARMS_MIN_PEAK_SEPARATION);
        min_peak_separation = LuaObject::getLuaFloat(L, -1, true, min_peak_separation, NULL);
        lua_pop(L, 1);

        /* highest peak ratio */
        lua_getfield(L, index, BATHY_PARMS_HIGHEST_PEAK_RATIO);
        highest_peak_ratio = LuaObject::getLuaFloat(L, -1, true, highest_peak_ratio, NULL);
        lua_pop(L, 1);

        /* surface width */
        lua_getfield(L, index, BATHY_PARMS_SURFACE_WIDTH);
        surface_width = LuaObject::getLuaFloat(L, -1, true, surface_width, NULL);
        lua_pop(L, 1);

        /* model as poisson */
        lua_getfield(L, index, BATHY_PARMS_MODEL_AS_POISSON);
        model_as_poisson = LuaObject::getLuaBoolean(L, -1, true, model_as_poisson, NULL);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * reader_t::toJson
 *----------------------------------------------------------------------------*/
const char* BathyParms::reader_t::toJson (void) const
{
    return "";
}

/*----------------------------------------------------------------------------
 * refraction_t::fromLua
 *----------------------------------------------------------------------------*/
void BathyParms::refraction_t::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        /* refraction index of air */
        lua_getfield(L, index, BATHY_PARMS_RI_AIR);
        ri_air = LuaObject::getLuaFloat(L, -1, true, ri_air, NULL);
        lua_pop(L, 1);

        /* prefraction index of water */
        lua_getfield(L, index, BATHY_PARMS_RI_WATER);
        ri_water = LuaObject::getLuaFloat(L, -1, true, ri_water, NULL);
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * refraction_t::toJson
 *----------------------------------------------------------------------------*/
const char* BathyParms::refraction_t::toJson (void) const
{
    return "";
}

/*----------------------------------------------------------------------------
 * uncertainty_t::fromLua
 *----------------------------------------------------------------------------*/
void BathyParms::uncertainty_t::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        /* assetKd */
        lua_getfield(L, index, BATHY_PARMS_ASSET_KD);
        const char* assetkd_name = LuaObject::getLuaString(L, -1, true, BATHY_PARMS_DEFAULT_ASSETKD);
        assetKd = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetkd_name, Asset::OBJECT_TYPE));
        if(!assetKd) throw RunTimeException(CRITICAL, RTE_ERROR, "Unable to find asset %s", assetkd_name);
        lua_pop(L, 1);

        /* resource Kd */
        lua_getfield(L, index, BATHY_PARMS_RESOURCE_KD);
        resourceKd = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, resourceKd, NULL));
        lua_pop(L, 1);
    }
}

/*----------------------------------------------------------------------------
 * uncertainty_t::toJson
 *----------------------------------------------------------------------------*/
const char* BathyParms::uncertainty_t::toJson (void) const
{
    return "";
}

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
 * init
 *----------------------------------------------------------------------------*/
void BathyParms::init (void)
{
    RECDEF(phRecType,       phRecDef,       sizeof(photon_t),       NULL);
    RECDEF(exRecType,       exRecDef,       sizeof(extent_t),       NULL /* "extent_id" */);
}

/*----------------------------------------------------------------------------
 * tojson
 *----------------------------------------------------------------------------*/
const char* BathyParms::tojson (void) const
{
    FString json_contents(R"({)"
    R"("%s":%s,)"
    R"("%s":%s,)"
    R"("%s":"%s")"
    R"(})",
    BATHY_PARMS_READER, reader.toJson(),
    BATHY_PARMS_REFRACTION, refraction.toJson(),
    BATHY_PARMS_UNCERTAINTY, uncertainty.toJson());

    return json_contents.c_str(true);
}

/*----------------------------------------------------------------------------
 * str2classifier
 *----------------------------------------------------------------------------*/
BathyParms::classifier_t BathyParms::str2classifier (const char* str)
{
    if(StringLib::match(str, "qtrees"))             return QTREES;
    if(StringLib::match(str, "coastnet"))           return COASTNET;
    if(StringLib::match(str, "openoceans++"))       return OPENOCEANSPP;
    if(StringLib::match(str, "medianfilter"))       return MEDIANFILTER;
    if(StringLib::match(str, "cshelph"))            return CSHELPH;
    if(StringLib::match(str, "bathypathfinder"))    return BATHYPATHFINDER;
    if(StringLib::match(str, "pointnet2"))          return POINTNET;
    if(StringLib::match(str, "openoceans"))         return OPENOCEANS;
    if(StringLib::match(str, "ensemble"))           return ENSEMBLE;
    return INVALID_CLASSIFIER;
}

/*----------------------------------------------------------------------------
 * classifier2str
 *----------------------------------------------------------------------------*/
const char* BathyParms::classifier2str (classifier_t classifier)
{
    switch(classifier)
    {
        case QTREES:            return "qtrees";
        case COASTNET:          return "coastnet";
        case OPENOCEANSPP:      return "openoceans++";
        case MEDIANFILTER:      return "medianfilter";
        case CSHELPH:           return "cshelph";
        case BATHYPATHFINDER:   return "bathypathfinder";
        case POINTNET:          return "pointnet";
        case OPENOCEANS:        return "openoceans";
        case ENSEMBLE:          return "ensemble";
        default:                return "unknown";
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyParms::BathyParms(lua_State* L, int index):
    Icesat2Parms (L, index)
{
    /* bathy parms */
    lua_getfield(L, index, BATHY_PARMS);
    if(lua_istable(L, index))
    {
        /* reader parms */
        lua_getfield(L, index, BATHY_PARMS_READER);
        reader.fromLua(L, -1);   
        lua_pop(L, 1);

        /* refraction parms */
        lua_getfield(L, index, BATHY_PARMS_REFRACTION);
        refraction.fromLua(L, -1);   
        lua_pop(L, 1);

        /* uncertainty parms */
        lua_getfield(L, index, BATHY_PARMS_UNCERTAINTY);
        uncertainty.fromLua(L, -1);   
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyParms::~BathyParms (void)
{
}

/*----------------------------------------------------------------------------
 * getSpotList
 *----------------------------------------------------------------------------*/
void BathyParms::getSpotList (lua_State* L, int index, bool* provided, bool* spots, int size)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of spots or a single spot */
    if(lua_istable(L, index))
    {
        /* Clear spot table (sets all to false) */
        memset(spots, 0, sizeof(bool) * size);
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
                if(spot >= 1 && spot <= Icesat2Parms::NUM_SPOTS)
                {
                    spots[spot-1] = true;
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
        memset(spots, 0, sizeof(bool) * size);

        /* Set spot */
        const int spot = LuaObject::getLuaInteger(L, -1);
        if(spot >= 1 && spot <= Icesat2Parms::NUM_SPOTS)
        {
            spots[spot-1] = true;
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
 * getClassifiers
 *----------------------------------------------------------------------------*/
void BathyParms::getClassifiers (lua_State* L, int index, bool* provided, bool* classifiers, int size)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of classifiers or a single classifier as a string */
    if(lua_istable(L, index))
    {
        /* Clear classifier table (sets all to false) */
        memset(classifiers, 0, sizeof(bool) * size);

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
        memset(classifiers, 0, sizeof(bool) * size);

        /* Set classifier */
        const int classifier = LuaObject::getLuaInteger(L, -1);
        if(classifier >= 0 && classifier < NUM_CLASSIFIERS)
        {
            if(provided) *provided = true;
            classifiers[classifier] = true;
        }
        else
        {
            mlog(ERROR, "Invalid classifier: %d", classifier);
        }
    }
    else if(lua_isstring(L, index))
    {
        /* Clear classifiers table (sets all to false) */
        memset(classifiers, 0, sizeof(bool) * size);

        /* Set classifier */
        const char* classifier_str = LuaObject::getLuaString(L, index);
        const classifier_t classifier = str2classifier(classifier_str);
        if(classifier != INVALID_CLASSIFIER)
        {
            if(provided) *provided = true;
            classifiers[static_cast<int>(classifier)] = true;
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
