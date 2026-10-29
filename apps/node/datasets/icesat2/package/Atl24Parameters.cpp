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
#include "Atl24Parameters.h"
#include "FieldMap.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl24Parameters::LUA_META_NAME = "Atl24Parameters";

 /******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - Atl24Fields
 *----------------------------------------------------------------------------*/
Atl24Fields::Atl24Fields():
    FieldMap<Field>({ {"compact",               &compact,               "Boolean flag indicating that only a compact set of fields should be returned"},
                      {"class_ph",              &class_ph,              "List of ATL24 photon classifications that are to be included in the response"},
                      {"confidence_threshold",  &confidence_threshold,  "Minimum confidence value of bathymetry photons to be returned (0.0 to 1.0)"},
                      {"night",                 &night,                 "Boolean flag controlling whether photons collected at night should be returned"},
                      {"anc_fields",            &anc_fields,            "Ancillary fields from the source granules to include in the response"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - Atl24Fields
 *----------------------------------------------------------------------------*/
void Atl24Fields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        FieldMap<Field>::fromLua(L, index);

        if(night.anyDisabled())
        {
            compact = false;
        }

        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void Atl24Parameters::fromLua (lua_State* L, int index)
{
    Icesat2Parameters::fromLua(L, index);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl24Parameters::Atl24Parameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name):
    Icesat2Parameters (L, key_space, asset_name, _resource, lua_meta_name)
{
    addParameter("atl24", &atl24, "Configuration structure for the 'atl24x' dataset construction");
    addParameter("quality_ph", &qualityPh, "Quality classification based on an ATL03 algorithms that attempt to identify instrumental artifacts, can be supplied as a single value (which means the classification must be exactly that), or a list (which means the classification must be in the list)");
}
