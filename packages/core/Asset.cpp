/*
 * Licensed to the University of Washington under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The University of Washington
 * licenses this file to you under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Asset.h"
#include "Dictionary.h"
#include "List.h"
#include "Ordering.h"
#include "LogLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Dictionary<Asset*> Asset::assets;
Mutex                       Asset::assetsMut;

const char* Asset::OBJECT_TYPE = "Asset";
const char* Asset::LuaMetaName = "Asset";
const struct luaL_Reg Asset::LuaMetaTable[] = {
    {"info",        luaInfo},
    {"load",        luaLoad},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Asset::~Asset (void)
{
    /* Remove Asset from Dictionary */
    if(registered)
    {
        registered = false;
        assetsMut.lock();
        {
            assets.remove(name);
        }
        assetsMut.unlock();
    }

    /* Delete Members */
    delete [] name;
    delete [] format;
    delete [] url;
}

/*----------------------------------------------------------------------------
 * luaCreate - create(<name>, [<format>, <url>])
 *----------------------------------------------------------------------------*/
int Asset::luaCreate (lua_State* L)
{
    try
    {
        bool alias = false;

        /* Get Required Parameters */
        const char* name = getLuaString(L, 1);

        /* Determine if Asset Exists */
        Asset* asset = NULL;
        assetsMut.lock();
        {
            if(assets.find(name))
            {
                asset = assets.get(name);
                associateMetaTable(L, LuaMetaName, LuaMetaTable);
                alias = true;
            }
        }
        assetsMut.unlock();

        /* Check if Asset Needs to be Created */
        if(asset == NULL)
        {
            const char* format      = getLuaString(L, 2);
            const char* url         = getLuaString(L, 3);
            asset = new Asset(L, name, format, url);
        }        

        /* Return Asset Object */
        return createLuaObject(L, asset, alias);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * operator[]
 *----------------------------------------------------------------------------*/
Asset::resource_t& Asset::operator[](int i)
{
    return resources[i];
}

/*----------------------------------------------------------------------------
 * size
 *----------------------------------------------------------------------------*/
int Asset::size(void)
{
    return resources.length();
}
/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Asset::Asset (lua_State* L, const char* _name, const char* _format, const char* _url):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    /* Configure LuaObject Name */
    ObjectName  = StringLib::duplicate(_name);

    /* Initialize Members */
    name        = StringLib::duplicate(_name);
    format      = StringLib::duplicate(_format);
    url         = StringLib::duplicate(_url);

    /* Register Asset */
    assetsMut.lock();
    {
        Asset* asset = this;
        if(assets.add(name, asset, true))
        {
            registered = true;
        }
        else
        {
            registered = false;
            mlog(CRITICAL, "Failed to register asset %s\n", name);
        }
    }
    assetsMut.unlock();
}

/*----------------------------------------------------------------------------
 * luaInfo - :info() --> name, format, url
 *----------------------------------------------------------------------------*/
int Asset::luaInfo (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Asset* lua_obj = (Asset*)getLuaSelf(L, 1);

        /* Push Info */
        lua_pushlstring(L, lua_obj->name,   StringLib::size(lua_obj->name));
        lua_pushlstring(L, lua_obj->format, StringLib::size(lua_obj->format));
        lua_pushlstring(L, lua_obj->url,    StringLib::size(lua_obj->url));

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error retrieving asset: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, 4);
}

/*----------------------------------------------------------------------------
 * luaLoad - :load(resource, attributes) --> boolean status
 *----------------------------------------------------------------------------*/
int Asset::luaLoad (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Asset* lua_obj = (Asset*)getLuaSelf(L, 1);

        /* Get Resource */
        const char* resource_name = getLuaString(L, 2);

        /* Create Resource */
        resource_t resource;
        StringLib::copy(&resource.name[0], resource_name, RESOURCE_NAME_MAX_LENGTH);

        /* Populate Attributes from Table */
        lua_pushnil(L);  // first key
        while (lua_next(L, 3) != 0)
        {
            double value = 0.0;
            bool provided = false;

            const char* key = getLuaString(L, -2);
            const char* str = getLuaString(L, -1, true, NULL, &provided);

            if(!provided) value = getLuaFloat(L, -1);
            else provided = StringLib::str2double(str, &value);

            if(provided)
            {
                if(!resource.attributes.add(key, value, true))
                {
                    mlog(CRITICAL, "Failed to populate duplicate attribute %s for resource %s\n", key, resource_name);
                }
            }
            else
            {
                mlog(DEBUG, "Unable to populate attribute %s for resource %s\n", key, resource_name);
            }

            lua_pop(L, 1); // removes 'value'; keeps 'key' for next iteration
        }

        /* Register Resource */
        lua_obj->resources.add(resource);

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error loading resource: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
