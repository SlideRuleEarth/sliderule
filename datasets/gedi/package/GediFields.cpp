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
#include "FieldEnumeration.h"
#include "RequestFields.h"
#include "GediFields.h"

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>, [<default asset>], [<default resource>])
 *----------------------------------------------------------------------------*/
int GediFields::luaCreate (lua_State* L)
{
    GediFields* gedi_fields = NULL;

    try
    {
        const char* default_asset_name = LuaObject::getLuaString(L, 2, true, NULL);
        const char* default_resource = LuaObject::getLuaString(L, 3, true, NULL);

        gedi_fields = new GediFields(L, default_asset_name, default_resource);
        gedi_fields->fromLua(L, 1);

        return createLuaObject(L, gedi_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        delete gedi_fields;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
GediFields::GediFields(lua_State* L , const char* default_asset_name, const char* default_resource):
    RequestFields (L, { {"asset",               &asset},
                        {"resource",            &resource},
                        {"beams",               &beams},
                        {"degrade_filter",      &degrade_filter},
                        {"l2_quality_filter",   &l2_quality_filter},
                        {"l4_quality_filter",   &l4_quality_filter},
                        {"surface_filter",      &surface_filter} }),
    asset(default_asset_name)
{
    if(default_resource)
    {
        resource = default_resource;
    }
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson - beam_t
 *----------------------------------------------------------------------------*/
string convertToJson(const GediFields::beam_t& v)
{
    switch(v)
    {
        case GediFields::BEAM0000:  return "\"beam0\"";
        case GediFields::BEAM0001:  return "\"beam1\"";
        case GediFields::BEAM0010:  return "\"beam2\"";
        case GediFields::BEAM0011:  return "\"beam3\"";
        case GediFields::BEAM0101:  return "\"beam5\"";
        case GediFields::BEAM0110:  return "\"beam6\"";
        case GediFields::BEAM1000:  return "\"beam8\"";
        case GediFields::BEAM1011:  return "\"beam11\"";
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam number: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua - beam_t
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const GediFields::beam_t& v)
{
    switch(v)
    {
        case GediFields::BEAM0000:  lua_pushstring(L, "beam0"); break;
        case GediFields::BEAM0001:  lua_pushstring(L, "beam1"); break;
        case GediFields::BEAM0010:  lua_pushstring(L, "beam2"); break;
        case GediFields::BEAM0011:  lua_pushstring(L, "beam3"); break;
        case GediFields::BEAM0101:  lua_pushstring(L, "beam5"); break;
        case GediFields::BEAM0110:  lua_pushstring(L, "beam6"); break;
        case GediFields::BEAM1000:  lua_pushstring(L, "beam8"); break;
        case GediFields::BEAM1011:  lua_pushstring(L, "beam11"); break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam number: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua - beam_t
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, GediFields::beam_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<GediFields::beam_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "beam0"))     v = GediFields::BEAM0000;
        else if(StringLib::match(str, "beam1"))     v = GediFields::BEAM0001;
        else if(StringLib::match(str, "beam2"))     v = GediFields::BEAM0010;
        else if(StringLib::match(str, "beam3"))     v = GediFields::BEAM0011;
        else if(StringLib::match(str, "beam5"))     v = GediFields::BEAM0101;
        else if(StringLib::match(str, "beam6"))     v = GediFields::BEAM0110;
        else if(StringLib::match(str, "beam8"))     v = GediFields::BEAM1000;
        else if(StringLib::match(str, "beam11"))    v = GediFields::BEAM1011;
        else throw RunTimeException(CRITICAL, RTE_ERROR, "beam number is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_ERROR, "beam number is an invalid type: %d", lua_type(L, index));
    }
}

/*----------------------------------------------------------------------------
 * convertToIndex - beam_t
 *----------------------------------------------------------------------------*/
int convertToIndex(const GediFields::beam_t& v)
{
    switch(v)
    {
        case GediFields::BEAM0000:  return 0;
        case GediFields::BEAM0001:  return 1;
        case GediFields::BEAM0010:  return 2;
        case GediFields::BEAM0011:  return 3;
        case GediFields::BEAM0101:  return 4;
        case GediFields::BEAM0110:  return 5;
        case GediFields::BEAM1000:  return 6;
        case GediFields::BEAM1011:  return 7;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam number: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertFromIndex - beam_t
 *----------------------------------------------------------------------------*/
void convertFromIndex(int index, GediFields::beam_t& v)
{
    switch(index)
    {
        case 0:  v = GediFields::BEAM0000; break;
        case 1:  v = GediFields::BEAM0001; break;
        case 2:  v = GediFields::BEAM0010; break;
        case 3:  v = GediFields::BEAM0011; break;
        case 4:  v = GediFields::BEAM0101; break;
        case 5:  v = GediFields::BEAM0110; break;
        case 6:  v = GediFields::BEAM1000; break;
        case 7:  v = GediFields::BEAM1011; break;
        default: throw RunTimeException(CRITICAL, RTE_ERROR, "invalid beam index: %d", index);
    }
}
