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

#include "core.h"
#include "ArrowParms.h"


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* ArrowParms::SELF                = "output";
const char* ArrowParms::PATH                = "path";
const char* ArrowParms::FORMAT              = "format";
const char* ArrowParms::OPEN_ON_COMPLETE    = "open_on_complete";
const char* ArrowParms::ASSET               = "asset";
const char* ArrowParms::REGION              = "region";
const char* ArrowParms::CREDENTIALS         = "credentials";

const char* ArrowParms::OBJECT_TYPE = "ArrowParms";
const char* ArrowParms::LuaMetaName = "ArrowParms";
const struct luaL_Reg ArrowParms::LuaMetaTable[] = {
    {"isnative",    luaIsNative},
    {"isfeather",   luaIsFeather},
    {"isparquet",   luaIsParquet},
    {"iscsv",       luaIsCSV},
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
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ArrowParms::ArrowParms (lua_State* L, int index):
    LuaObject           (L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    path                (NULL),
    format              (NATIVE),
    open_on_complete    (false),
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

            /* Output Format */
            lua_getfield(L, index, FORMAT);
            format = str2outputformat(LuaObject::getLuaString(L, -1, true, NULL, &field_provided));
            if(field_provided) mlog(DEBUG, "Setting %s to %d", FORMAT, (int)format);
            lua_pop(L, 1);

            /* Output Open on Complete */
            lua_getfield(L, index, OPEN_ON_COMPLETE);
            open_on_complete = LuaObject::getLuaBoolean(L, -1, true, open_on_complete, &field_provided);
            if(field_provided) mlog(DEBUG, "Setting %s to %d", OPEN_ON_COMPLETE, (int)open_on_complete);
            lua_pop(L, 1);

            /* Asset */
            lua_getfield(L, index, ASSET);
            asset_name = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL, &field_provided));
            if(field_provided) mlog(DEBUG, "Setting %s to %s", ASSET, asset_name);
            lua_pop(L, 1);

            #ifdef __aws__
            /* Region */
            lua_getfield(L, index, REGION);
            region = StringLib::duplicate(LuaObject::getLuaString(L, -1, true, NULL, &field_provided));
            if(region)
            {
                mlog(DEBUG, "Setting %s to %s", REGION, region);
            }
            else if(asset_name != NULL)
            {
                Asset* asset = (Asset*)LuaObject::getLuaObjectByName(asset_name, Asset::OBJECT_TYPE);
                region = StringLib::duplicate(asset->getRegion());
                asset->releaseLuaObject();
            }
            lua_pop(L, 1);

            /* AWS Credentials */
            lua_getfield(L, index, CREDENTIALS);
            credentials.fromLua(L, -1);
            if(credentials.provided)
            {
                mlog(DEBUG, "Setting %s from user", CREDENTIALS);
            }
            else if(asset_name != NULL)
            {
                credentials = CredentialStore::get(asset_name);
                if(credentials.provided)
                {
                    mlog(DEBUG, "Setting %s from asset %s", CREDENTIALS, asset_name);
                }
            }
            lua_pop(L, 1);
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
    if     (!fmt_str)                               return UNSUPPORTED;
    else if(StringLib::match(fmt_str, "native"))    return NATIVE;
    else if(StringLib::match(fmt_str, "feather"))   return FEATHER;
    else if(StringLib::match(fmt_str, "parquet"))   return PARQUET;
    else if(StringLib::match(fmt_str, "csv"))       return CSV;
    else                                            return UNSUPPORTED;
}

/*----------------------------------------------------------------------------
 * luaIsNative
 *----------------------------------------------------------------------------*/
int ArrowParms::luaIsNative (lua_State* L)
{
    try
    {
        ArrowParms* lua_obj = (ArrowParms*)getLuaSelf(L, 1);
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
        ArrowParms* lua_obj = (ArrowParms*)getLuaSelf(L, 1);
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
        ArrowParms* lua_obj = (ArrowParms*)getLuaSelf(L, 1);
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
        ArrowParms* lua_obj = (ArrowParms*)getLuaSelf(L, 1);
        return returnLuaStatus(L, lua_obj->format == CSV);
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
        ArrowParms* lua_obj = (ArrowParms*)getLuaSelf(L, 1);
        if(lua_obj->path) lua_pushstring(L, lua_obj->path);
        else lua_pushnil(L);
        return 1;
    }
    catch(const RunTimeException& e)
    {
        return luaL_error(L, "method invoked from invalid object: %s", __FUNCTION__);
    }
}
