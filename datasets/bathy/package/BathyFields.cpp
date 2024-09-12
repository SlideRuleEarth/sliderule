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

#include "geo.h"
#include "OsApi.h"
#include "BathyFields.h"

/******************************************************************************
 * DATA
 ******************************************************************************/

/* Photon Record Definition */
const char* BathyFields::phRecType = "bathyrec.photons";
const RecordObject::fieldDef_t BathyFields::phRecDef[] = {
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
const char* BathyFields::exRecType = "bathyrec";
const RecordObject::fieldDef_t BathyFields::exRecDef[] = {
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
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int BathyFields::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Requests parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new BathyFields(L, 1));
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
void BathyFields::init (void)
{
    RECDEF(phRecType,   phRecDef,   sizeof(photon_t),   NULL);
    RECDEF(exRecType,   exRecDef,   sizeof(extent_t),   NULL /* "extent_id" */);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyFields::BathyFields(lua_State* L, int index):
    Icesat2Fields (L, index, {  {"asset09",             &atl09AssetName},
                                {"resource09",          &atl09Resource},
                                {"max_dem_delta",       &maxDemDelta},
                                {"min_dem_delta",       &minDemDelta},
                                {"ph_in_extent",        &phInExtent},
                                {"generate_ndwi",       &generateNdwi},
                                {"use_bathy_mask",      &useBathyMask},
                                {"find_sea_surface",    &findSeaSurface},
                                {"classifiers",         &classifiers},
                                {"spots",               &spots},
                                {"surface",             &surface},
                                {"refraction",          &refraction},
                                {"uncertainty",         &uncertainty} })
{
    /* ATL09 asset */
    asset09 = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(atl09AssetName.value.c_str(), Asset::OBJECT_TYPE));
    if(!asset09) throw RunTimeException(CRITICAL, RTE_ERROR, "unable to find asset %s", atl09AssetName.value.c_str());
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
BathyFields::~BathyFields (void)
{
    if(asset09) asset09->releaseLuaObject();
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToLua - classifier_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const BathyFields::classifier_t& v)
{
    switch(v)
    {
        case BathyFields::QTREES:           lua_pushstring(L, BathyFields::QTREES_NAME);            break;
        case BathyFields::COASTNET:         lua_pushstring(L, BathyFields::COASTNET_NAME);          break;
        case BathyFields::OPENOCEANSPP:     lua_pushstring(L, BathyFields::OPENOCEANSPP_NAME);      break;
        case BathyFields::MEDIANFILTER:     lua_pushstring(L, BathyFields::MEDIANFILTER_NAME);      break;
        case BathyFields::CSHELPH:          lua_pushstring(L, BathyFields::CSHELPH_NAME);           break;
        case BathyFields::BATHYPATHFINDER:  lua_pushstring(L, BathyFields::BATHYPATHFINDER_NAME);   break;
        case BathyFields::POINTNET:         lua_pushstring(L, BathyFields::POINTNET_NAME);          break;
        case BathyFields::OPENOCEANS:       lua_pushstring(L, BathyFields::OPENOCEANS_NAME);        break;
        case BathyFields::ENSEMBLE:         lua_pushstring(L, BathyFields::ENSEMBLE_NAME);          break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid classifier: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - classifier_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, BathyFields::classifier_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<BathyFields::classifier_t>(LuaObject::getLuaInteger(L, index));
    }    
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, BathyFields::QTREES_NAME))            v = BathyFields::QTREES;
        else if(StringLib::match(str, BathyFields::COASTNET_NAME))          v = BathyFields::COASTNET;
        else if(StringLib::match(str, BathyFields::OPENOCEANSPP_NAME))      v = BathyFields::OPENOCEANSPP;
        else if(StringLib::match(str, BathyFields::MEDIANFILTER_NAME))      v = BathyFields::MEDIANFILTER;
        else if(StringLib::match(str, BathyFields::CSHELPH_NAME))           v = BathyFields::CSHELPH;
        else if(StringLib::match(str, BathyFields::BATHYPATHFINDER_NAME))   v = BathyFields::BATHYPATHFINDER;
        else if(StringLib::match(str, BathyFields::POINTNET_NAME))          v = BathyFields::POINTNET;
        else if(StringLib::match(str, BathyFields::OPENOCEANS_NAME))        v = BathyFields::OPENOCEANS;
        else if(StringLib::match(str, BathyFields::ENSEMBLE_NAME))          v = BathyFields::ENSEMBLE;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "classifier is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "classifier is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - classifier_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const BathyFields::classifier_t& v)
{
    return static_cast<int>(v);
}

/*----------------------------------------------------------------------------
 * convertFromIndex - classifier_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, BathyFields::classifier_t& v)
{
    v = static_cast<BathyFields::classifier_t>(index);
}
