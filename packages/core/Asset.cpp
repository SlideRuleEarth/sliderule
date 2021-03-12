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

#include "Asset.h"
#include "Dictionary.h"
#include "List.h"
#include "Ordering.h"
#include "EventLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

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
 * luaCreate - create(<name>, <format>, <url>, <index>)
 *----------------------------------------------------------------------------*/
int Asset::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* _name = getLuaString(L, 1);
        const char* _format = getLuaString(L, 2);
        const char* _url    = getLuaString(L, 3);
        const char* _index  = getLuaString(L, 4);

        /* Return Asset Object */
        return createLuaObject(L, new Asset(L, _name, _format, _url, _index));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Asset::~Asset (void)
{
    /* Delete Members */
    delete [] name;
    delete [] format;
    delete [] url;
    delete [] index;
}

/*----------------------------------------------------------------------------
 * load
 *----------------------------------------------------------------------------*/
int Asset::load (resource_t& resource)
{
    return resources.add(resource);    
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
 * getName
 *----------------------------------------------------------------------------*/
const char* Asset::getName (void)
{
    return name;
}

/*----------------------------------------------------------------------------
 * getFormat
 *----------------------------------------------------------------------------*/
const char* Asset::getFormat (void)
{
    return format;
}

/*----------------------------------------------------------------------------
 * getUrl
 *----------------------------------------------------------------------------*/
const char* Asset::getUrl (void)
{
    return url;
}

/*----------------------------------------------------------------------------
 * getIndex
 *----------------------------------------------------------------------------*/
const char* Asset::getIndex (void)
{
    return index;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Asset::Asset (lua_State* L, const char* _name, const char* _format, const char* _url, const char* _index):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    /* Configure LuaObject Name */
    ObjectName  = StringLib::duplicate(_name);

    /* Initialize Members */
    name    = StringLib::duplicate(_name);
    format  = StringLib::duplicate(_format);
    url     = StringLib::duplicate(_url);
    index   = StringLib::duplicate(_index);
}

/*----------------------------------------------------------------------------
 * luaInfo - :info() --> name, format, url, index
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
        lua_pushlstring(L, lua_obj->index,  StringLib::size(lua_obj->index));

        /* Set Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error retrieving asset: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, 5);
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
        StringLib::copy(&resource.name[0], resource_name, RESOURCE_NAME_LENGTH);

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
        lua_obj->load(resource);

        /* Set Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error loading resource: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
