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
 * Constructor - Atl03GranuleFields
 *----------------------------------------------------------------------------*/
GediGranuleFields::GediGranuleFields():
    FieldDictionary({ {"year",      &year},
                      {"doy",       &doy},
                      {"orbit",     &orbit},
                      {"region",    &region},
                      {"track",     &track},
                      {"version",   &version} })
{
}

/*----------------------------------------------------------------------------
 * parseResource
 *
 *  GEDI02_A_2019108185228_O01971_03_T00922_02_003_01_V002.h5
 *    - GEDI02_A = Product Short Name
 *    - 2019108 = Julian Date of Acquisition in YYYYDDD
 *    - 185228 = Hours, Minutes and Seconds of Acquisition (HHMMSS)
 *    - O01971 = O = Orbit, 01971 = Orbit Number
 *    - 03 = Sub-Orbit Granule Number (1-4)
 *    - T00922 = T = Track, 00922 = Track Number
 *    - 02 = Positioning and Pointing Determination System (PPDS) type (00 is predict, 01 rapid, 02 and higher is final.)
 *    - 003 = PGE Version Number
 *    - 01 = Granule Production Version
 *    - V002 = LP DAAC Release Number
 *----------------------------------------------------------------------------*/
void GediGranuleFields::parseResource (const char* resource)
{
    long val;

    /* check resource */
    const int strsize = StringLib::size(resource);
    if( strsize < 57 || 
        resource[0] != 'G' || resource[1] !='E' || resource[2] != 'D' || resource[3] != 'E' ||
        resource[23] != 'O' || resource[33] != 'T' || resource[50] != 'V')
    {
        return; // not a GEDI standard data product
    }

    /* get year */
    char year_str[5];
    year_str[0] = resource[9];
    year_str[1] = resource[10];
    year_str[2] = resource[11];
    year_str[3] = resource[12];
    year_str[4] = '\0';
    if(StringLib::str2long(year_str, &val, 10))
    {
        year = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse year from resource %s: %s", resource, year_str);
    }

    /* get doy */
    char doy_str[4];
    doy_str[0] = resource[13];
    doy_str[1] = resource[14];
    doy_str[2] = resource[15];
    doy_str[3] = '\0';
    if(StringLib::str2long(doy_str, &val, 10))
    {
        doy = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse day of year from resource %s: %s", resource, doy_str);
    }

    /* get orbit */
    char orbit_str[6];
    orbit_str[0] = resource[24];
    orbit_str[1] = resource[25];
    orbit_str[2] = resource[26];
    orbit_str[3] = resource[27];
    orbit_str[4] = resource[28];
    orbit_str[5] = '\0';
    if(StringLib::str2long(orbit_str, &val, 10))
    {
        orbit = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse orbit from resource %s: %s", resource, orbit_str);
    }

    /* get region */
    char region_str[2];
    region_str[0] = resource[31];
    region_str[1] = '\0';
    if(StringLib::str2long(region_str, &val, 10))
    {
        region = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse region from resource %s: %s", resource, region_str);
    }

    /* get track */
    char track_str[6];
    track_str[0] = resource[34];
    track_str[1] = resource[35];
    track_str[2] = resource[36];
    track_str[3] = resource[37];
    track_str[4] = resource[38];
    track_str[5] = '\0';
    if(StringLib::str2long(track_str, &val, 10))
    {
        track = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse track from resource %s: %s", resource, track_str);
    }

    /* get version */
    char version_str[4];
    version_str[0] = resource[51];
    version_str[1] = resource[52];
    version_str[2] = resource[53];
    version_str[3] = '\0';
    if(StringLib::str2long(version_str, &val, 10))
    {
        version = static_cast<uint8_t>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse version from resource %s: %s", resource, version_str);
    }
}

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>, <key_space>, [<default asset>], [<default resource>])
 *----------------------------------------------------------------------------*/
int GediFields::luaCreate (lua_State* L)
{
    GediFields* gedi_fields = NULL;

    try
    {
        const uint64_t key_space = LuaObject::getLuaInteger(L, 2, true, RequestFields::DEFAULT_KEY_SPACE);
        const char* asset_name = LuaObject::getLuaString(L, 3, true, NULL);
        const char* _resource = LuaObject::getLuaString(L, 4, true, NULL);

        gedi_fields = new GediFields(L, key_space, asset_name, _resource);
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
GediFields::GediFields(lua_State* L , uint64_t key_space, const char* asset_name, const char* _resource):
    RequestFields (L, key_space, asset_name, _resource,
        { {"beams",             &beams},
          {"degrade_filter",    &degrade_filter},
          {"l2_quality_filter", &l2_quality_filter},
          {"l4_quality_filter", &l4_quality_filter},
          {"surface_filter",    &surface_filter},
          {"anc_fields",        &anc_fields},
          {"granule",           &granule_fields},
          // backwards compatibility
          {"beam",              &beams},
          {"degrade_flag",      &degrade_flag},
          {"l2_quality_flag",   &l2_quality_flag},
          {"l4_quality_flag",   &l4_quality_flag},
          {"surface_flag",      &surface_flag} })
{
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void GediFields::fromLua (lua_State* L, int index)
{
    RequestFields::fromLua(L, index);

    // set filters
    if(degrade_flag == 1) degrade_filter = true;
    if(l2_quality_flag == 1) l2_quality_filter = true;
    if(l4_quality_flag == 1) l4_quality_filter = true;
    if(surface_flag == 1) surface_filter = true;

    // parse resource name
    if(!resource.value.empty())
    {
        granule_fields.parseResource(resource.value.c_str());
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
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam number: %d", static_cast<int>(v));
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
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam number: %d", static_cast<int>(v));
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
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "beam number is an invalid value: %d", static_cast<int>(v));
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "beam number is an invalid type: %d", lua_type(L, index));
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
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam number: %d", static_cast<int>(v));
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
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid beam index: %d", index);
    }
}
