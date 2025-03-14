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

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Asset::OBJECT_TYPE = "Asset";
const char* Asset::LUA_META_NAME = "Asset";
const struct luaL_Reg Asset::LUA_META_TABLE[] = {
    {"info",        luaInfo},
    {"load",        luaLoad},
    {NULL,          NULL}
};

Mutex Asset::ioDriverMut;
Dictionary<Asset::io_driver_t> Asset::ioDrivers;

/******************************************************************************
 * VOID IO DRIVER CLASS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * create
 *----------------------------------------------------------------------------*/
Asset::IODriver* Asset::IODriver::create (const Asset* _asset, const char* resource)
{
    (void)_asset;
    (void)resource;
    return new IODriver();
}

/*----------------------------------------------------------------------------
 * ioRead
 *----------------------------------------------------------------------------*/
int64_t Asset::IODriver::ioRead (uint8_t* data, int64_t size, uint64_t pos)
{
    (void)data;
    (void)size;
    (void)pos;
    return 0;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Asset::IODriver::IODriver (void) = default;

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Asset::IODriver::~IODriver (void) = default;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<name>, <identity>, <driver>, <path>, [<index>], [<region>], [<endpoint>])
 *----------------------------------------------------------------------------*/
int Asset::luaCreate (lua_State* L)
{
    try
    {
        attributes_t _attributes;

        /* Get Parameters */
        _attributes.name       = getLuaString(L, 1);
        _attributes.identity   = getLuaString(L, 2);
        _attributes.driver     = getLuaString(L, 3);
        _attributes.path       = getLuaString(L, 4);
        _attributes.index      = getLuaString(L, 5, true, NULL);
        _attributes.region     = getLuaString(L, 6, true, NULL);
        _attributes.endpoint   = getLuaString(L, 7, true, NULL);

        /* Get IO Driver */
        io_driver_t _driver;
        bool found = false;
        ioDriverMut.lock();
        {
            found = ioDrivers.find(_attributes.driver, &_driver);
        }
        ioDriverMut.unlock();

        /* Check Driver */
        if(!found)
        {
            mlog(CRITICAL, "Failed to find I/O driver for %s, using default driver", _attributes.driver);
            _driver.factory = Asset::IODriver::create; // set it to the default
        }

        /* Return Asset Object */
        return createLuaObject(L, new Asset(L, _attributes, _driver));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * registerDriver
 *----------------------------------------------------------------------------*/
bool Asset::registerDriver (const char* _format, io_driver_f factory)
{
    bool status;

    ioDriverMut.lock();
    {
        const io_driver_t driver = { .factory = factory };
        status = ioDrivers.add(_format, driver);
        mlog(DEBUG, "Registering driver %s: %d", _format, status);
    }
    ioDriverMut.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * createDriver
 *----------------------------------------------------------------------------*/
Asset::IODriver* Asset::createDriver (const char* resource) const
{
    if(driver.factory) return driver.factory(this, resource);
    return NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Asset::~Asset (void)
{
    delete [] attributes.name;
    delete [] attributes.identity;
    delete [] attributes.driver;
    delete [] attributes.path;
    delete [] attributes.index;
    delete [] attributes.region;
    delete [] attributes.endpoint;
}

/*----------------------------------------------------------------------------
 * load
 *----------------------------------------------------------------------------*/
int Asset::load (const resource_t& resource)
{
    return resources.add(resource);
}

/*----------------------------------------------------------------------------
 * operator[]
 *----------------------------------------------------------------------------*/
Asset::resource_t& Asset::operator[](int i)
{
    return resources.get(i);
}

/*----------------------------------------------------------------------------
 * size
 *----------------------------------------------------------------------------*/
int Asset::size(void) const
{
    return resources.length();
}

/*----------------------------------------------------------------------------
 * getName
 *----------------------------------------------------------------------------*/
const char* Asset::getName (void) const
{
    return attributes.name;
}

/*----------------------------------------------------------------------------
 * getIdentity
 *----------------------------------------------------------------------------*/
const char* Asset::getIdentity (void) const
{
    return attributes.identity;
}

/*----------------------------------------------------------------------------
 * getDriver
 *----------------------------------------------------------------------------*/
const char* Asset::getDriver (void) const
{
    return attributes.driver;
}

/*----------------------------------------------------------------------------
 * getPath
 *----------------------------------------------------------------------------*/
const char* Asset::getPath (void) const
{
    return attributes.path;
}

/*----------------------------------------------------------------------------
 * getIndex
 *----------------------------------------------------------------------------*/
const char* Asset::getIndex (void) const
{
    return attributes.index;
}

/*----------------------------------------------------------------------------
 * getRegion
 *----------------------------------------------------------------------------*/
const char* Asset::getRegion (void) const
{
    return attributes.region;
}

/*----------------------------------------------------------------------------
 * getEndpoint
 *----------------------------------------------------------------------------*/
const char* Asset::getEndpoint (void) const
{
    return attributes.endpoint;
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Asset::Asset (lua_State* L, const attributes_t& _attributes, const io_driver_t& _driver):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    driver(_driver),
    resources(ASSET_STARTING_RESOURCES_PER_INDEX)
{
    attributes.name     = StringLib::duplicate(_attributes.name);
    attributes.identity = StringLib::duplicate(_attributes.identity);
    attributes.driver   = StringLib::duplicate(_attributes.driver);
    attributes.path     = StringLib::duplicate(_attributes.path);
    attributes.index    = StringLib::duplicate(_attributes.index);
    attributes.region   = StringLib::duplicate(_attributes.region);
    attributes.endpoint = StringLib::duplicate(_attributes.endpoint);
}

/*----------------------------------------------------------------------------
 * luaInfo - :info() --> name, identity, driver, path, index, region, endpoint, status
 *----------------------------------------------------------------------------*/
int Asset::luaInfo (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        Asset* lua_obj = dynamic_cast<Asset*>(getLuaSelf(L, 1));
        attributes_t* attr = &lua_obj->attributes;

        /* Push Info */
        attr->name      ? (void)lua_pushlstring(L, attr->name,      StringLib::size(attr->name))        : lua_pushnil(L);
        attr->identity  ? (void)lua_pushlstring(L, attr->identity,  StringLib::size(attr->identity))    : lua_pushnil(L);
        attr->driver    ? (void)lua_pushlstring(L, attr->driver,    StringLib::size(attr->driver))      : lua_pushnil(L);
        attr->path      ? (void)lua_pushlstring(L, attr->path,      StringLib::size(attr->path))        : lua_pushnil(L);
        attr->index     ? (void)lua_pushlstring(L, attr->index,     StringLib::size(attr->index))       : lua_pushnil(L);
        attr->region    ? (void)lua_pushlstring(L, attr->region,    StringLib::size(attr->region))      : lua_pushnil(L);
        attr->endpoint  ? (void)lua_pushlstring(L, attr->endpoint,  StringLib::size(attr->endpoint))    : lua_pushnil(L);

        /* Set Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error retrieving asset: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, 8);
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
        Asset* lua_obj = dynamic_cast<Asset*>(getLuaSelf(L, 1));

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
                    mlog(CRITICAL, "Failed to populate duplicate attribute %s for resource %s", key, resource_name);
                }
            }
            else
            {
                mlog(DEBUG, "Unable to populate attribute %s for resource %s", key, resource_name);
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
        mlog(e.level(), "Error loading resource: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
