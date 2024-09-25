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
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "ArrowFields.h"

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowFields::ArrowFields (void):
    FieldDictionary ({
        {"path",                &path},
        {"format",              &format},
        {"open_on_complete",    &openOnComplete},
        {"as_geo",              &asGeo},
        {"with_checksum",       &withChecksum},
        {"with_validation",     &withValidation},
        {"asset_name",          &assetName},
        {"region",              &region},
        #ifdef __aws__
        {"credentials",         &credentials},
        #endif
    })
{
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void ArrowFields::fromLua (lua_State* L, int index)
{
    FieldDictionary::fromLua(L, index);    

    // check format
    if(format.value == ArrowFields::PARQUET && asGeo)
    {
        format.value = ArrowFields::GEOPARQUET;
    }
    else if(format.value == ArrowFields::GEOPARQUET && !asGeo)
    {
        asGeo = true;
    }

    // handle asset
    #ifdef __aws__
    if(!assetName.value.empty())
    {
        /* Get Asset */
        Asset* asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetName.value.c_str(), Asset::OBJECT_TYPE));
        region = StringLib::duplicate(asset->getRegion());
        credentials = CredentialStore::get(asset->getIdentity());
        asset->releaseLuaObject();
    }
    #endif
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson
 *----------------------------------------------------------------------------*/
string convertToJson(const ArrowFields::format_t& v)
{
    switch(v)
    {
        case ArrowFields::FEATHER:      return "\"feather\"";
        case ArrowFields::PARQUET:      return "\"parquet\"";
        case ArrowFields::GEOPARQUET:   return "\"geoparquet\"";
        case ArrowFields::CSV:          return "\"csv\"";
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid format: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const ArrowFields::format_t& v)
{
    switch(v)
    {
        case ArrowFields::FEATHER:      lua_pushstring(L, "feather");       break;
        case ArrowFields::PARQUET:      lua_pushstring(L, "parquet");       break;
        case ArrowFields::GEOPARQUET:   lua_pushstring(L, "geoparquet");    break;
        case ArrowFields::CSV:          lua_pushstring(L, "csv");           break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid format: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, ArrowFields::format_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<ArrowFields::format_t>(LuaObject::getLuaInteger(L, index));
    }    
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
             if(StringLib::match(str, "feather"))       v = ArrowFields::FEATHER;
        else if(StringLib::match(str, "parquet"))       v = ArrowFields::PARQUET;
        else if(StringLib::match(str, "geoparquet"))    v = ArrowFields::GEOPARQUET;
        else if(StringLib::match(str, "csv"))           v = ArrowFields::CSV;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "format is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "format is an invalid type: %d", lua_type(L, index));
    }
}

