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
#include "CasalsFields.h"

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor - Atl03GranuleFields
 *----------------------------------------------------------------------------*/
CasalsGranuleFields::CasalsGranuleFields():
    FieldDictionary({ {"year",      &year},
                      {"month",     &month},
                      {"day",       &day},
                      {"version",   &version} })
{
}

/*----------------------------------------------------------------------------
 * parseResource
 *
 *  casals_l1b_20241112T163934_001_02.h5
 *    - casals_l1b = Product Short Name
 *    - 20241112 = Julian Date of Acquisition in YYYYMMDD
 *    - 001 = Release
 *    - 02 =  Version
 *----------------------------------------------------------------------------*/
void CasalsGranuleFields::parseResource (const char* resource)
{
    long val;

    /* check resource */
    const int strsize = StringLib::size(resource);
    if( strsize < 57 ||
        resource[0] != 'c' || resource[1] !='a' || resource[2] != 's' ||
        resource[3] != 'a' || resource[4] != 'l' || resource[5] != 's')
    {
        return; // not a CASALS standard data product
    }

    /* get year */
    char year_str[5];
    year_str[0] = resource[11];
    year_str[1] = resource[12];
    year_str[2] = resource[13];
    year_str[3] = resource[14];
    year_str[4] = '\0';
    if(StringLib::str2long(year_str, &val, 10))
    {
        year = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse year from resource %s: %s", resource, year_str);
    }

    /* get month */
    char month_str[3];
    month_str[0] = resource[15];
    month_str[1] = resource[16];
    month_str[2] = '\0';
    if(StringLib::str2long(month_str, &val, 10))
    {
        month = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse month from resource %s: %s", resource, month_str);
    }

    /* get day */
    char day_str[3];
    day_str[0] = resource[17];
    day_str[1] = resource[18];
    day_str[2] = '\0';
    if(StringLib::str2long(day_str, &val, 10))
    {
        day = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse day from resource %s: %s", resource, day_str);
    }

    /* get version */
    char version_str[4];
    version_str[0] = resource[27];
    version_str[1] = resource[28];
    version_str[2] = resource[29];
    version_str[3] = '\0';
    if(StringLib::str2long(version_str, &val, 10))
    {
        version = static_cast<int>(val);
    }
    else
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to parse version from resource %s: %s", resource, version_str);
    }
}

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>, <key_space>, [<default asset>], [<default resource>])
 *----------------------------------------------------------------------------*/
int CasalsFields::luaCreate (lua_State* L)
{
    CasalsFields* casals_fields = NULL;

    try
    {
        const uint64_t key_space = LuaObject::getLuaInteger(L, 2, true, RequestFields::DEFAULT_KEY_SPACE);
        const char* asset_name = LuaObject::getLuaString(L, 3, true, NULL);
        const char* _resource = LuaObject::getLuaString(L, 4, true, NULL);

        casals_fields = new CasalsFields(L, key_space, asset_name, _resource);
        casals_fields->fromLua(L, 1);

        return createLuaObject(L, casals_fields);
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        delete casals_fields;
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CasalsFields::CasalsFields(lua_State* L , uint64_t key_space, const char* asset_name, const char* _resource):
    RequestFields (L, key_space, asset_name, _resource,
        { {"anc_fields",        &anc_fields},
          {"granule",           &granule_fields} })
{
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void CasalsFields::fromLua (lua_State* L, int index)
{
    RequestFields::fromLua(L, index);

    // parse resource name
    if(!resource.value.empty())
    {
        granule_fields.parseResource(resource.value.c_str());
    }
}
