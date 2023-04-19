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

Mutex Asset::ioDriverMut;
Dictionary<Asset::io_driver_t> Asset::ioDrivers;

/******************************************************************************
 * VOID IO DRIVER CLASS
 ******************************************************************************/

const char* Asset::IODriver::FORMAT = "nil";

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
Asset::IODriver::IODriver (void)
{
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Asset::IODriver::~IODriver (void)
{
}

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<name>, <driver>, <path>, <index>)
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
        io_driver_t _io_driver = NULL;
        ioDriverMut.lock();
        {
            ioDrivers.find(_attributes.driver, &_io_driver);
        }
        ioDriverMut.unlock();

        /* Check Driver */
        if(_io_driver == NULL)
        {
            mlog(CRITICAL, "Failed to find I/O driver for %s, using default driver", _attributes.driver);
            _io_driver = Asset::IODriver::create; // set it to the default
        }

        /* Return Asset Object */
        return createLuaObject(L, new Asset(L, _attributes, _io_driver));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * pythonCreate
 *----------------------------------------------------------------------------*/
Asset* Asset::pythonCreate (const char* name, const char* identity, const char* driver, const char* path, const char* index, const char* region, const char* endpoint)
{
    attributes_t _attributes;

    /* Get Parameters */
    _attributes.name       = name;
    _attributes.identity   = identity;
    _attributes.driver     = driver;
    _attributes.path       = path;
    _attributes.index      = index;
    _attributes.region     = region;
    _attributes.endpoint   = endpoint;

    /* Get IO Driver */
    io_driver_t _io_driver = NULL;
    ioDriverMut.lock();
    {
        ioDrivers.find(_attributes.driver, &_io_driver);
    }
    ioDriverMut.unlock();

    /* Return Asset Object */
    if(_io_driver == NULL)  return NULL;
    else                    return new Asset(NULL, _attributes, _io_driver);
}

/*----------------------------------------------------------------------------
 * registerDriver
 *----------------------------------------------------------------------------*/
bool Asset::registerDriver (const char* _format, io_driver_t driver)
{
    bool status;

    ioDriverMut.lock();
    {
        status = ioDrivers.add(_format, driver);
    }
    ioDriverMut.unlock();

    return status;
}

/*----------------------------------------------------------------------------
 * createDriver
 *----------------------------------------------------------------------------*/
Asset::IODriver* Asset::createDriver (const char* resource) const
{
    if(io_driver)   return io_driver(this, resource);
    else            return NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Asset::~Asset (void)
{
    if(attributes.name)     delete [] attributes.name;
    if(attributes.identity) delete [] attributes.identity;
    if(attributes.driver)   delete [] attributes.driver;
    if(attributes.path)     delete [] attributes.path;
    if(attributes.index)    delete [] attributes.index;
    if(attributes.region)   delete [] attributes.region;
    if(attributes.endpoint) delete [] attributes.endpoint;
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
Asset::Asset (lua_State* L, attributes_t _attributes, io_driver_t _io_driver):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    attributes.name     = StringLib::duplicate(_attributes.name);
    attributes.identity = StringLib::duplicate(_attributes.identity);
    attributes.driver   = StringLib::duplicate(_attributes.driver);
    attributes.path     = StringLib::duplicate(_attributes.path);
    attributes.index    = StringLib::duplicate(_attributes.index);
    attributes.region   = StringLib::duplicate(_attributes.region);
    attributes.endpoint = StringLib::duplicate(_attributes.endpoint);
    io_driver           = _io_driver;
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
        Asset* lua_obj = (Asset*)getLuaSelf(L, 1);
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
    return returnLuaStatus(L, status, 7);
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
