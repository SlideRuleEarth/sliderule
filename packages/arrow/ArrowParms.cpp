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
#include "ArrowParms.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowParms::SELF                = "output";
const char* ArrowParms::PATH                = "path";
const char* ArrowParms::FORMAT              = "format";
const char* ArrowParms::OPEN_ON_COMPLETE    = "open_on_complete";
const char* ArrowParms::AS_GEO              = "as_geo";
const char* ArrowParms::WITH_CHECKSUM       = "with_checksum";
const char* ArrowParms::ANCILLARY           = "ancillary";
const char* ArrowParms::ASSET               = "asset";
const char* ArrowParms::REGION              = "region";
const char* ArrowParms::CREDENTIALS         = "credentials";

const char* ArrowParms::OBJECT_TYPE = "ArrowParms";
const char* ArrowParms::LUA_META_NAME = "ArrowParms";
const struct luaL_Reg ArrowParms::LUA_META_TABLE[] = {
    {"isnative",    luaIsNative},
    {"isfeather",   luaIsFeather},
    {"isparquet",   luaIsParquet},
    {"iscsv",       luaIsCSV},
    {"isarrow",     luaIsArrow}, // combination of isfeather, iscsv, isparquet
    {"path",        luaPath},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<parameter table>)
 *----------------------------------------------------------------------------*/
int ArrowParms::luaCreate (lua_State* L)
{
    try
    {
        /* Check if Lua Table */
        if(lua_type(L, 1) != LUA_TTABLE)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "Arrow parameters must be supplied as a lua table");
        }

        /* Return Request Parameter Object */
        return createLuaObject(L, new ArrowParms(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowParms::ArrowParms (lua_State* L, int index):
    LuaObject           (L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    path                (NULL),
    format              (NATIVE),
    open_on_complete    (false),
    as_geo              (true),
    with_checksum       (false),
    asset_name          (NULL),
    region              (NULL)
{
    /* Populate Object from Lua */
    try
    {
        /* Must be a Table */
        if(lua_istable(L, index))
        {
            bool field_provided = false;

            /* Output Path */
            lua_getfield(L, index, PATH);
            path = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, path, &field_provided));
            if(field_provided) mlog(DEBUG, "Setting %s to %s", PATH, path);
            lua_pop(L, 1);

            /* Output Open on Complete */
            lua_getfield(L, index, OPEN_ON_COMPLETE);
            open_on_complete = LuaObject::getLuaBoolean(L, -1, true, open_on_complete, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", OPEN_ON_COMPLETE, (int)open_on_complete);
            lua_pop(L, 1);

            /* As Geo */
            lua_getfield(L, index, AS_GEO);
            as_geo = LuaObject::getLuaBoolean(L, -1, true, as_geo, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", AS_GEO, (int)as_geo);
            lua_pop(L, 1);

            /* With Checksum */
            lua_getfield(L, index, WITH_CHECKSUM);
            with_checksum = LuaObject::getLuaBoolean(L, -1, true, with_checksum, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", WITH_CHECKSUM, (int)with_checksum);
            lua_pop(L, 1);

            /* Output Format */
            lua_getfield(L, index, FORMAT);
            const char* output_format = LuaObject::getLuaString(L, -1, true, NULL, &field_provided);
            format = str2outputformat(output_format);
            if(field_provided)
            {
                mlog(DEBUG, "Setting %s to %d", FORMAT, (int)format);
                if((format == PARQUET) && StringLib::match(output_format, "geoparquet"))
                {
                    /* Special Case: override as_geo when geoparquet format specified */
                    as_geo = true;
                }
            }
            lua_pop(L, 1);

            /* Ancillary */
            lua_getfield(L, index, ANCILLARY);
            luaGetAncillary(L, -1, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to user provided list", ANCILLARY);
            lua_pop(L, 1);

            /* Asset */
            lua_getfield(L, index, ASSET);
            asset_name = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL, &field_provided));
            if(field_provided) mlog(DEBUG, "Setting %s to %s", ASSET, asset_name);
            lua_pop(L, 1);

            #ifdef __aws__
            if(asset_name)
            {
                /* Get Asset */
                Asset* asset = dynamic_cast<Asset*>(LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE));

                /* Region */
                region = StringLib::duplicate(asset->getRegion());
                if(region) mlog(DEBUG, "Setting %s to %s from asset %s", REGION, region, asset_name);
                else mlog(ERROR, "Failed to get region from asset %s", asset_name);

                /* Credentials */
                credentials = CredentialStore::get(asset->getIdentity());
                if(!credentials.expiration.value.empty()) mlog(DEBUG, "Setting %s from asset %s", CREDENTIALS, asset_name);
                else mlog(ERROR, "Failed to get credentials from asset %s", asset_name);

                /* Release Asset */
                asset->releaseLuaObject();
            }
            else
            {
                /* Region */
                lua_getfield(L, index, REGION);
                region = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL, &field_provided));
                if(region) mlog(DEBUG, "Setting %s to %s", REGION, region);
                lua_pop(L, 1);

                /* AWS Credentials */
                lua_getfield(L, index, CREDENTIALS);
                credentials.fromLua(L, -1);
                if(!credentials.expiration.value.empty()) mlog(DEBUG, "Setting %s from user", CREDENTIALS);
                lua_pop(L, 1);
            }
            #endif
        }
    }
    catch(const RunTimeException& e)
    {
        cleanup();
        throw;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ArrowParms::~ArrowParms (void)
{
    cleanup();
}

/*----------------------------------------------------------------------------
 * tojson
 *----------------------------------------------------------------------------*/
const char* ArrowParms::tojson (void) const
{
    rapidjson::Document doc;
    doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();
    rapidjson::Value nullval(rapidjson::kNullType);

    if(path) doc.AddMember("path", rapidjson::Value(path, allocator), allocator);
    else     doc.AddMember("path", nullval, allocator);

    doc.AddMember("format", rapidjson::Value(format2str(format), allocator), allocator);

    doc.AddMember("open_on_complete", open_on_complete, allocator);
    doc.AddMember("as_geo", as_geo, allocator);

    if(asset_name) doc.AddMember("asset_name", rapidjson::Value(asset_name, allocator), allocator);
    else           doc.AddMember("asset_name", nullval, allocator);

    if(region) doc.AddMember("region", rapidjson::Value(region, allocator), allocator);
    else       doc.AddMember("region", nullval, allocator);

    /* Serialize ancillary fields */
    rapidjson::Value ancillary(rapidjson::kArrayType);
    for(int i = 0; i < (int)ancillary_fields.size(); i++)
    {
        ancillary.PushBack(rapidjson::Value(ancillary_fields[i].c_str(), allocator), allocator);
    }
    doc.AddMember("ancillary_fields", ancillary, allocator);

    #ifdef __aws__
    doc.AddMember("credentials", rapidjson::Value(!credentials.expiration.value.empty() ? "provided" : "not provided", allocator), allocator);
    #endif

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);

    return StringLib::duplicate(buffer.GetString());
}

/*----------------------------------------------------------------------------
 * cleanup
 *----------------------------------------------------------------------------*/
void ArrowParms::cleanup (void)
{
    if(path)
    {
        delete [] path;
        path = NULL;
    }

    if(asset_name)
    {
        delete [] asset_name;
        asset_name = NULL;
    }

    if(region)
    {
        delete [] region;
        region = NULL;
    }
}

/*----------------------------------------------------------------------------
 * str2outputformat
 *----------------------------------------------------------------------------*/
ArrowParms::format_t ArrowParms::str2outputformat (const char* fmt_str)
{
    if(!fmt_str)                                return UNSUPPORTED;
    if(StringLib::match(fmt_str, "native"))     return NATIVE;
    if(StringLib::match(fmt_str, "feather"))    return FEATHER;
    if(StringLib::match(fmt_str, "parquet"))    return PARQUET;
    if(StringLib::match(fmt_str, "geoparquet")) return PARQUET;
    if(StringLib::match(fmt_str, "csv"))        return CSV;
    return UNSUPPORTED;
}

/*----------------------------------------------------------------------------
 * luaIsNative
 *----------------------------------------------------------------------------*/
int ArrowParms::luaIsNative (lua_State* L)
{
    try
    {
        const ArrowParms* lua_obj = dynamic_cast<ArrowParms*>(getLuaSelf(L, 1));
        return returnLuaStatus(L, lua_obj->format == NATIVE);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaIsFeather
 *----------------------------------------------------------------------------*/
int ArrowParms::luaIsFeather (lua_State* L)
{
    try
    {
        const ArrowParms* lua_obj = dynamic_cast<ArrowParms*>(getLuaSelf(L, 1));
        return returnLuaStatus(L, lua_obj->format == FEATHER);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaIsParquet
 *----------------------------------------------------------------------------*/
int ArrowParms::luaIsParquet (lua_State* L)
{
    try
    {
        const ArrowParms* lua_obj = dynamic_cast<ArrowParms*>(getLuaSelf(L, 1));
        return returnLuaStatus(L, lua_obj->format == PARQUET);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaIsCSV
 *----------------------------------------------------------------------------*/
int ArrowParms::luaIsCSV (lua_State* L)
{
    try
    {
        const ArrowParms* lua_obj = dynamic_cast<ArrowParms*>(getLuaSelf(L, 1));
        return returnLuaStatus(L, lua_obj->format == CSV);
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaIsArrow
 *----------------------------------------------------------------------------*/
int ArrowParms::luaIsArrow (lua_State* L)
{
    try
    {
        const ArrowParms* lua_obj = dynamic_cast<ArrowParms*>(getLuaSelf(L, 1));
        return returnLuaStatus(L, (lua_obj->format == PARQUET) ||
                                  (lua_obj->format == CSV) ||
                                  (lua_obj->format == FEATHER));
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaPath
 *----------------------------------------------------------------------------*/
int ArrowParms::luaPath (lua_State* L)
{
    try
    {
        const ArrowParms* lua_obj = dynamic_cast<ArrowParms*>(getLuaSelf(L, 1));
        if(lua_obj->path) lua_pushstring(L, lua_obj->path);
        else lua_pushnil(L);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}

/*----------------------------------------------------------------------------
 * luaGetAncillary
 *----------------------------------------------------------------------------*/
void ArrowParms::luaGetAncillary (lua_State* L, int index, bool* provided)
{
    /* Reset Provided */
    if(provided) *provided = false;

    /* Must be table of strings */
    if(lua_istable(L, index))
    {
        /* Get number of fields in table */
        const int num_fields = lua_rawlen(L, index);
        if(num_fields > 0 && provided) *provided = true;

        /* Iterate through each field in table */
        for(int i = 0; i < num_fields; i++)
        {
            /* Get field */
            lua_rawgeti(L, index, i+1);

            /* Set field */
            if(lua_isstring(L, -1))
            {
                const char* field_str = LuaObject::getLuaString(L, -1);
                const string field_name(field_str);
                ancillary_fields.push_back(field_name);
            }

            /* Clean up stack */
            lua_pop(L, 1);
        }
    }
    else if(!lua_isnil(L, index))
    {
        mlog(ERROR, "ancillary fields must be provided as a table of strings");
    }
}

/*----------------------------------------------------------------------------
 * format2str
 *----------------------------------------------------------------------------*/
const char* ArrowParms::format2str (format_t fmt)
{
    switch(fmt)
    {
        case NATIVE:    return "NATIVE";
        case FEATHER:   return "FEATHER";
        case PARQUET:   return "PARQUET";
        case CSV:       return "CSV";
        default:        return "UNSUPPORTED";
    }
}
