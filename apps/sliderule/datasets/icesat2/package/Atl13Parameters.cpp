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
#include "Atl13Parameters.h"
#include "LuaObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Atl13Parameters::LUA_META_NAME = "Atl13Parameters";

 /******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - Atl13Fields
 *----------------------------------------------------------------------------*/
Atl13Fields::Atl13Fields():
    FieldMap<Field>({ {"refid",         &reference_id,      "Selects data associated only with this ATL13 Reference ID of the inland body of water"},
                      {"name",          &name,              "Selects data associated only with this inland body of water name"},
                      {"coord",         &coordinate,        "Selects data associated only with the inland body of water that contains this coordinate"},
                      {"anc_fields",    &anc_fields,        "Ancillary fields from the source granules to include in the response"} }),
    provided(false)
{
}

/*----------------------------------------------------------------------------
 * fromLua - Atl13Fields
 *----------------------------------------------------------------------------*/
void Atl13Fields::fromLua (lua_State* L, int index)
{
    if(lua_istable(L, index))
    {
        FieldMap<Field>::fromLua(L, index);
        provided = true;
    }
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void Atl13Parameters::fromLua (lua_State* L, int index)
{
    Icesat2Parameters::fromLua(L, index);
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Atl13Parameters::Atl13Parameters(lua_State* L, uint64_t key_space, const char* asset_name, const char* _resource, const char* lua_meta_name):
    Icesat2Parameters (L, key_space, asset_name, _resource, lua_meta_name)
{
    addParameter("atl13", &atl13, "Configuration structure for the 'atl13x' dataset construction");
}
