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

#include "OsApi.h"
#include "Atl08Parameters.h"
#include "FieldMap.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl08Parameters::LUA_META_NAME = "Atl08Parameters";

 /******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void Atl08Parameters::fromLua (lua_State* L, int index)
{
    Icesat2Parameters::fromLua(L, index);

    // check for te quality filter
    lua_getfield(L, index, "te_quality_filter");
    te_quality_filter_provided = !lua_isnil(L, -1);
    lua_pop(L, 1);

    // check for can quality filter
    lua_getfield(L, index, "can_quality_filter");
    can_quality_filter_provided = !lua_isnil(L, -1);
    lua_pop(L, 1);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl08Parameters::Atl08Parameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name):
    Icesat2Parameters (L, key_space, asset_name, _resource, lua_meta_name),
    te_quality_filter_provided(false),
    can_quality_filter_provided(false)
{
    addParameter("use_abs_h",           &use_abs_h,             "Read absolute (instead of relative) height metrics; i.e. return ellipsoidal elevations instead of relief measurements");
    addParameter("te_quality_filter",   &te_quality_filter,     "Filter out low quality terrain photons when calculating metrics");
    addParameter("can_quality_filter",  &can_quality_filter,    "Filter out low quality canopy photons when calculating metrics");
    addParameter("atl08_fields",        &atl08Fields,           "Ancillary fields in the 'land_segments' group of the ATL08 granule to include in the response");
}
