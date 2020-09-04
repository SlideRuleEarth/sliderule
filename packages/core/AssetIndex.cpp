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

#include "AssetIndex.h"
#include "Dictionary.h"
#include "LogLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Dictionary<AssetIndex*> AssetIndex::assets;
Mutex                   AssetIndex::assetsMut;

const char* AssetIndex::OBJECT_TYPE = "AssetIndex";
const char* AssetIndex::LuaMetaName = "AssetIndex";
const struct luaL_Reg AssetIndex::LuaMetaTable[] = {
    {"info",        luaInfo},
    {"load",        luaLoad},
    {NULL,          NULL}
};


/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<name>, [<format>, <url>])
 *----------------------------------------------------------------------------*/
int AssetIndex::luaCreate (lua_State* L)
{
    try
    {
        bool alias = false;

        /* Get Required Parameters */
        const char* name = getLuaString(L, 1);

        /* Determine if AssetIndex Exists */
        AssetIndex* asset = NULL;
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

        /* Check if AssetIndex Needs to be Created */
        if(asset == NULL)
        {
            const char* format      = getLuaString(L, 2);
            const char* url         = getLuaString(L, 3);
            asset = new AssetIndex(L, name, format, url);
        }        

        /* Return AssetIndex Object */
        return createLuaObject(L, asset, alias);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
AssetIndex::AssetIndex (lua_State* L, const char* _name, const char* _format, const char* _url):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    /* Initialize Members */
    name        = StringLib::duplicate(_name);
    format      = StringLib::duplicate(_format);
    url         = StringLib::duplicate(_url);

    /* Register AssetIndex */
    assetsMut.lock();
    {
        AssetIndex* asset = this;
        if(assets.add(name, asset, true))
        {
            registered = true;
        }
        else
        {
            registered = false;
            mlog(CRITICAL, "Failed to register asset %s; likely race condition if duplicate assets defined\n", name);
        }
    }
    assetsMut.unlock();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
AssetIndex::~AssetIndex (void)
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
 * luaInfo - :info() --> name, format, url
 *----------------------------------------------------------------------------*/
int AssetIndex::luaInfo (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex* lua_obj = (AssetIndex*)getLuaSelf(L, 1);

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
int AssetIndex::luaLoad (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        AssetIndex* lua_obj = (AssetIndex*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* resource = getLuaString(L, 2);

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
