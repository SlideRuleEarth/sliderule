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

#include "DeviceObject.h"
#include "Ordering.h"
#include "EventLib.h"
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
        while(key != INVALID_KEY)
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error sending data: %s\n", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error receiving data: %s\n", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error getting configuration: %s\n", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error determining if connected: %s\n", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error closing connection: %s\n", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
