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
#include "OutputFields.h"
#include "SystemConfig.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* OutputFields::PARMS = "output";

/******************************************************************************
 * CLASS METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
OutputFields::OutputFields (void):
    FieldDictionary ({
        {"path",                &path},
        {"format",              &format},
        {"open_on_complete",    &openOnComplete},
        {"as_geo",              &asGeo},
        {"with_checksum",       &withChecksum},
        {"with_validation",     &withValidation},
        {"asset",               &assetName},
        {"region",              &region},
        #ifdef __aws__
        {"credentials",         &credentials},
        #endif
        {"ancillary",           &ancillaryFields},
        {"fields",              &finalFields}
    })
{
}

/*----------------------------------------------------------------------------
 * fromLua
 *----------------------------------------------------------------------------*/
void OutputFields::fromLua (lua_State* L, int index)
{
    FieldDictionary::fromLua(L, index);

    // check format
    if(format.value == OutputFields::PARQUET && asGeo)
    {
        format.value = OutputFields::GEOPARQUET;
    }
    else if(format.value == OutputFields::GEOPARQUET && !asGeo)
    {
        asGeo = true;
    }

    // handle asset
    if(!assetName.value.empty())
    {
        // get asset
        Asset* asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(assetName.value.c_str(), Asset::OBJECT_TYPE));
        if(asset)
        {
            // set region
            region = asset->getRegion();

            // set credentials
            #ifdef __aws__
            credentials = CredentialStore::get(asset->getIdentity());
            #endif

            // set output path
            const char* path_prefix = StringLib::match(asset->getDriver(), "s3") ? "s3://" : "";
            const char* path_suffix = "bin";
            if(format.value == GEOPARQUET)
            {
                path_suffix = ".geoparquet";
            }
            else if(format.value == PARQUET)
            {
                path_suffix = ".parquet";
            }
            else if(format == CSV)
            {
                path_suffix = ".csv";
            }
            else if(format == H5)
            {
                path_suffix = ".h5";
            }
            else if(format == LAS)
            {
                path_suffix = ".las";
            }
            else if(format == LAZ)
            {
                path_suffix = ".laz";
            }
            if(!path.value.empty() && (path.value[0] != '\0'))
            {
                path = FString("%s%s/%s", path_prefix, asset->getPath(), path.value.c_str()).c_str();
            }
            else
            {
                const FString path_name("%s.%016lX%s", SystemConfig::settings().organization.value.c_str(), OsApi::time(OsApi::CPU_CLK), path_suffix);
                path = FString("%s%s/%s", path_prefix, asset->getPath(), path_name.c_str()).c_str();
            }
            mlog(INFO, "Generating unique path: %s", path.value.c_str());

            // release asset
            asset->releaseLuaObject();
        }
    }
    else if(path.value.empty() || (path.value[0] == '\0'))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "Unable to determine output path");
    }
}

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * convertToJson
 *----------------------------------------------------------------------------*/
string convertToJson(const OutputFields::format_t& v)
{
    switch(v)
    {
        case OutputFields::FEATHER:      return "\"feather\"";
        case OutputFields::PARQUET:      return "\"parquet\"";
        case OutputFields::GEOPARQUET:   return "\"geoparquet\"";
        case OutputFields::CSV:          return "\"csv\"";
        case OutputFields::H5:           return "\"h5\"";
        case OutputFields::LAS:          return "\"las\"";
        case OutputFields::LAZ:          return "\"laz\"";
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid format: %d", static_cast<int>(v));
    }
}

/*----------------------------------------------------------------------------
 * convertToLua
 *----------------------------------------------------------------------------*/
int convertToLua(lua_State* L, const OutputFields::format_t& v)
{
    switch(v)
    {
        case OutputFields::FEATHER:      lua_pushstring(L, "feather");       break;
        case OutputFields::PARQUET:      lua_pushstring(L, "parquet");       break;
        case OutputFields::GEOPARQUET:   lua_pushstring(L, "geoparquet");    break;
        case OutputFields::CSV:          lua_pushstring(L, "csv");           break;
        case OutputFields::H5:           lua_pushstring(L, "h5");            break;
        case OutputFields::LAS:          lua_pushstring(L, "las");           break;
        case OutputFields::LAZ:          lua_pushstring(L, "laz");           break;
        default: throw RunTimeException(CRITICAL, RTE_FAILURE, "invalid format: %d", static_cast<int>(v));
    }

    return 1;
}

/*----------------------------------------------------------------------------
 * convertFromLua
 *----------------------------------------------------------------------------*/
void convertFromLua(lua_State* L, int index, OutputFields::format_t& v)
{
    if(lua_isinteger(L, index))
    {
        v = static_cast<OutputFields::format_t>(LuaObject::getLuaInteger(L, index));
    }
    else if(lua_isstring(L, index))
    {
        const char* str = LuaObject::getLuaString(L, index);
        if     (StringLib::match(str, "feather"))       v = OutputFields::FEATHER;
        else if(StringLib::match(str, "parquet"))       v = OutputFields::PARQUET;
        else if(StringLib::match(str, "geoparquet"))    v = OutputFields::GEOPARQUET;
        else if(StringLib::match(str, "csv"))           v = OutputFields::CSV;
        else if(StringLib::match(str, "h5"))            v = OutputFields::H5;
        else if(StringLib::match(str, "las"))           v = OutputFields::LAS;
        else if(StringLib::match(str, "laz"))           v = OutputFields::LAZ;
        else throw RunTimeException(CRITICAL, RTE_FAILURE, "format is an invalid value: %s", str);
    }
    else if(!lua_isnil(L, index))
    {
        throw RunTimeException(CRITICAL, RTE_FAILURE, "format is an invalid type: %d", lua_type(L, index));
    }
}
