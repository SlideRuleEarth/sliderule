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

#include "DeviceObject.h"
#include "Ordering.h"
#include "LogLib.h"
#include "OsApi.h"
#include "StringLib.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Ordering<DeviceObject*> DeviceObject::deviceList;
Mutex                   DeviceObject::deviceListMut;
okey_t                  DeviceObject::currentListKey = 0;

const char* DeviceObject::OBJECT_TYPE = "DeviceObject";
const char* DeviceObject::LuaMetaName = "DeviceObject";
const struct luaL_Reg DeviceObject::LuaMetaTable[] = {
    {"send",        luaSend},
    {"receive",     luaReceive},
    {"config",      luaConfig},
    {"connected",   luaIsConnected},
    {"close",       luaClose},
    {NULL,          NULL}
};

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
DeviceObject::DeviceObject (lua_State* L, role_t _role):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable),
    role(_role)
{
    /* Add Device to List */
    deviceListMut.lock();
    {
        DeviceObject* this_device = this;
        deviceList.add(currentListKey, this_device);
        deviceListKey = currentListKey++;
    }
    deviceListMut.unlock();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
DeviceObject::~DeviceObject (void)
{
    deviceListMut.lock();
    {
        deviceList.remove(deviceListKey);
    }
    deviceListMut.unlock();
}

/*----------------------------------------------------------------------------
 * getDeviceList
 *
 *  Notes: must free string that it returns
 *----------------------------------------------------------------------------*/
char* DeviceObject::getDeviceList(void)
{
    #define DEV_STR_SIZE 64
    char devstr[DEV_STR_SIZE];
    char* liststr;

    deviceListMut.lock();
    {
        int liststrlen = DEV_STR_SIZE * deviceList.length() + 1;
        liststr = new char[liststrlen];
        liststr[0] = '\0';

        DeviceObject* dev;
        okey_t key = deviceList.first(&dev);
        while(key != Ordering<DeviceObject*>::INVALID_KEY)
        {
            StringLib::format(devstr, DEV_STR_SIZE, "%c %s\n", dev->isConnected(0) ? 'C' : 'D', dev->getConfig());
            StringLib::concat(liststr, devstr, liststrlen);
            key = deviceList.next(&dev);
        }
    }
    deviceListMut.unlock();

    return liststr;
}

/*----------------------------------------------------------------------------
 * luaList - list()
 *----------------------------------------------------------------------------*/
int DeviceObject::luaList(lua_State* L)
{
    (void)L;
    const char* device_list_str = DeviceObject::getDeviceList();
    mlog(RAW, "%s", device_list_str);
    delete [] device_list_str;
    return 0;
}

/*----------------------------------------------------------------------------
 * luaSend - :send(<string>) --> success/fail
 *----------------------------------------------------------------------------*/
int DeviceObject::luaSend (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        DeviceObject* dev = (DeviceObject*)getLuaSelf(L, 1);

        /* Send Data */
        size_t str_len = 0;
        const char* str = lua_tolstring(L, 2, &str_len);
        int bytes = dev->writeBuffer(str, (int)str_len);

        /* Set Status */
        status = (bytes == (int)str_len);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error sending data: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaReceive - :receive() --> string
 *----------------------------------------------------------------------------*/
int DeviceObject::luaReceive (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        DeviceObject* dev = (DeviceObject*)getLuaSelf(L, 1);

        /* Receive Data */
        int io_maxsize = LocalLib::getIOMaxsize();
        char* packet = new char [io_maxsize];
        int bytes = dev->readBuffer(packet, io_maxsize);
        lua_pushlstring(L, packet, bytes);
        delete [] packet;

        /* Set Status */
        status = (bytes > 0);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error receiving data: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, 2);
}

/*----------------------------------------------------------------------------
 * luaConfig - :config() --> string
 *----------------------------------------------------------------------------*/
int DeviceObject::luaConfig (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        DeviceObject* dev = (DeviceObject*)getLuaSelf(L, 1);

        /* Get Configuration */
        const char* config = dev->getConfig();
        lua_pushstring(L, config);

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error getting configuration: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status, 2);
}

/*----------------------------------------------------------------------------
 * luaIsConnected - :connected() --> boolean
 *----------------------------------------------------------------------------*/
int DeviceObject::luaIsConnected (lua_State* L)
{
     bool status = false;

    try
    {
        /* Get Self */
        DeviceObject* dev = (DeviceObject*)getLuaSelf(L, 1);

        /* Set Status */
        status = dev->isConnected(1);
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error determining if connected: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaClose - :close() --> boolean
 *----------------------------------------------------------------------------*/
int DeviceObject::luaClose (lua_State* L)
{
     bool status = false;

    try
    {
        /* Get Self */
        DeviceObject* dev = (DeviceObject*)getLuaSelf(L, 1);

        /* Close Connection */
        dev->closeConnection();

        /* Set Status */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error closing connection: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
