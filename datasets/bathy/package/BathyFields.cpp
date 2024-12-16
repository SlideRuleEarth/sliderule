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
 * STATIC DATA
 ******************************************************************************/

const double BathyFields::NIGHT_SOLAR_ELEVATION_THRESHOLD = 5.0; // degrees
const double BathyFields::MINIMUM_HORIZONTAL_SUBAQUEOUS_UNCERTAINTY = 0.0; // meters
const double BathyFields::MINIMUM_VERTICAL_SUBAQUEOUS_UNCERTAINTY = 0.10; // meters
const double BathyFields::DEFAULT_WIND_SPEED = 3.3; // meters

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int BathyFields::luaCreate (lua_State* L)
{
    BathyFields* bathy_fields = NULL;

    try
    {
        const long key_space = LuaObject::getLuaInteger(L, 2, true, 0);
        const char* default_asset_name = LuaObject::getLuaString(L, 3, true, "icesat2");
        const char* default_resource = LuaObject::getLuaString(L, 4, true, NULL);

        bathy_fields = new BathyFields(L, key_space, default_asset_name, default_resource);
        bathy_fields->fromLua(L, 1);

        return createLuaObject(L, bathy_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        delete bathy_fields;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * luaCreate - classifier(<index>)
 *----------------------------------------------------------------------------*/
int BathyFields::luaClassifier (lua_State* L)
{
    try
    {
        BathyFields* lua_obj = dynamic_cast<BathyFields*>(getLuaSelf(L, 1));
        const classifier_t classifier = static_cast<classifier_t>(getLuaInteger(L, 2));
        lua_pushboolean(L, lua_obj->classifiers[classifier]);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error getting classifier state: %s", e.what());
        lua_pushboolean(L, false);
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void BathyFields::fromLua (lua_State* L, int index)
{
    Icesat2Fields::fromLua(L, index);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyFields::BathyFields(lua_State* L, uint64_t key_space, const char* default_asset_name, const char* default_resource):
    Icesat2Fields (L, key_space, default_asset_name, default_resource,
        { {"asset09",             &atl09AssetName},
          {"max_dem_delta",       &maxDemDelta},
          {"min_dem_delta",       &minDemDelta},
          {"max_geoid_delta",     &maxGeoidDelta},
          {"min_geoid_delta",     &minGeoidDelta},
          {"ph_in_extent",        &phInExtent},
          {"generate_ndwi",       &generateNdwi},
          {"use_bathy_mask",      &useBathyMask},
          {"classifiers",         &classifiers},
          {"spots",               &spots},
          {"surface",             &surface},
          {"refraction",          &refraction},
          {"uncertainty",         &uncertainty},
          {"coastnet",            &coastnet},
          {"qtrees",              &qtrees},
          {"openoceanspp",        &openoceanspp},
          {"coastnet_version",    &coastnetVersion},
          {"qtrees_version",      &qtreesVersion},
          {"openoceanspp_version",&openoceansppVersion} })
{
    LuaEngine::setAttrFunc(L, "classifier", luaClassifier);
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson - classifier_t
 *----------------------------------------------------------------------------*/
string convertToJson(const BathyFields::classifier_t& v)
{
    switch(v)
    {
        case BathyFields::QTREES:           return FString("\"%s\"", BathyFields::QTREES_NAME).c_str();
        case BathyFields::COASTNET:         return FString("\"%s\"", BathyFields::COASTNET_NAME).c_str();
        case BathyFields::OPENOCEANSPP:     return FString("\"%s\"", BathyFields::OPENOCEANSPP_NAME).c_str();
        case BathyFields::MEDIANFILTER:     return FString("\"%s\"", BathyFields::MEDIANFILTER_NAME).c_str();
        case BathyFields::CSHELPH:          return FString("\"%s\"", BathyFields::CSHELPH_NAME).c_str();
        case BathyFields::BATHYPATHFINDER:  return FString("\"%s\"", BathyFields::BATHYPATHFINDER_NAME).c_str();
        case BathyFields::POINTNET:         return FString("\"%s\"", BathyFields::POINTNET_NAME).c_str();
        case BathyFields::OPENOCEANS:       return FString("\"%s\"", BathyFields::OPENOCEANS_NAME).c_str();
        case BathyFields::ENSEMBLE:         return FString("\"%s\"", BathyFields::ENSEMBLE_NAME).c_str();
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid classifier: %d", static_cast<int>(v));
    }
}

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
