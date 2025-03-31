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
        const uint64_t key_space = LuaObject::getLuaInteger(L, 2, true, RequestFields::DEFAULT_KEY_SPACE);
        const char* asset_name = LuaObject::getLuaString(L, 3, true, "icesat2");
        const char* _resource = LuaObject::getLuaString(L, 4, true, NULL);

        bathy_fields = new BathyFields(L, key_space, asset_name, _resource);
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
 * fromLua
 *----------------------------------------------------------------------------*/
void BathyFields::fromLua (lua_State* L, int index)
{
    Icesat2Fields::fromLua(L, index);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
BathyFields::BathyFields(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource):
    Icesat2Fields (L, key_space, asset_name, _resource,
        { {"asset09",             &atl09AssetName},
          {"max_dem_delta",       &maxDemDelta},
          {"min_dem_delta",       &minDemDelta},
          {"max_geoid_delta",     &maxGeoidDelta},
          {"min_geoid_delta",     &minGeoidDelta},
          {"ph_in_extent",        &phInExtent},
          {"generate_ndwi",       &generateNdwi},
          {"use_bathy_mask",      &useBathyMask},
          {"spots",               &spots},
          {"surface",             &surface},
          {"refraction",          &refraction},
          {"uncertainty",         &uncertainty} })
{
}
