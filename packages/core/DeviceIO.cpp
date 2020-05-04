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

#include "DeviceIO.h"
#include "Dictionary.h"
#include "StringLib.h"
#include "OsApi.h"
#include "MsgQ.h"
#include "List.h"
#include "LogLib.h"
#include "LuaEngine.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* DeviceIO::OBJECT_TYPE = "DeviceIO";
const char* DeviceIO::LuaMetaName = "DeviceReader";
const struct luaL_Reg DeviceIO::LuaMetaTable[] = {
    {"stats",       luaLogPktStats},
    {"wait",        luaWaitOnConnect},
    {"block",       luaConfigBlock},
    {"dod",         luaDieOnDisconnect},
    {NULL,          NULL}
};

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
DeviceIO::DeviceIO(lua_State* L, DeviceObject* _device):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(_device);

    /* Initialize Thread */
    ioActive            = false;
    ioThread            = NULL;

    /* Initialize Parameters */
    device              = _device;
    dieOnDisconnect     = true;
    blockCfg            = SYS_TIMEOUT;

    /* Initialize Counters */
    bytesProcessed      = 0;
    bytesDropped        = 0;
    packetsProcessed    = 0;
    packetsDropped      = 0;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
DeviceIO::~DeviceIO(void)
{
    // full destruction is handled in child classes to guarantee
    // correct order in this destructor
}

/*----------------------------------------------------------------------------
 * luaLogPktStats - :stats([<log level>])
 *----------------------------------------------------------------------------*/
int DeviceIO::luaLogPktStats(lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        DeviceIO* lua_obj = (DeviceIO*)getLuaSelf(L, 1);

        /* Get Log Level */
        log_lvl_t lvl = (log_lvl_t)lua_tointeger(L, 2);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "processed (bytes)", lua_obj->bytesProcessed);
        LuaEngine::setAttrInt(L, "dropped (bytes)", lua_obj->bytesProcessed);
        LuaEngine::setAttrInt(L, "processed (packets)", lua_obj->packetsProcessed);
        LuaEngine::setAttrInt(L, "dropped (packets)", lua_obj->packetsDropped);

        /* Log Stats */
        mlog(lvl, "processed (bytes):   %d\n", lua_obj->bytesProcessed);
        mlog(lvl, "dropped (bytes):     %d\n", lua_obj->bytesDropped);
        mlog(lvl, "processed (packets): %d\n", lua_obj->packetsProcessed);
        mlog(lvl, "dropped (packets):   %d\n", lua_obj->packetsDropped);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error logging device I/O statistics: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaWaitOnConnect - :wait([<timeout in seconds>], [<number of connectinos>])
 *----------------------------------------------------------------------------*/
int DeviceIO::luaWaitOnConnect(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        DeviceIO* lua_obj = (DeviceIO*)getLuaSelf(L, 1);

        /* Get Parameters */
        long timeout_seconds        = getLuaInteger(L, 2, true, 5);
        long number_of_connections  = getLuaInteger(L, 3, true, 1);

        /* Check Device Exists */
        if(!lua_obj->device)
        {
            throw LuaException("device invalid... unable to execute command!");
        }

        /* Wait for Device */
        long count = timeout_seconds;
        while(!lua_obj->device->isConnected(number_of_connections) && ((timeout_seconds == -1) || (count-- > 0)))
        {
            LocalLib::sleep(1);
        }

        /* Check Success */
        if(!lua_obj->device->isConnected(number_of_connections))
        {
            throw LuaException ("timeout occurred waiting for connection on device");
        }

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error waiting on device: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaConfigBlock - :block(<enable or timeout>)
 *----------------------------------------------------------------------------*/
int DeviceIO::luaConfigBlock(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        DeviceIO* lua_obj = (DeviceIO*)getLuaSelf(L, 1);

        /* Get Block Value */
        if(lua_isboolean(L, 2))
        {
            bool enable = getLuaBoolean(L, 2);
            if(enable)  lua_obj->blockCfg = SYS_TIMEOUT;
            else        lua_obj->blockCfg = IO_CHECK;
        }
        else if(lua_isinteger(L, 2))
        {
            int timeout = (int)getLuaInteger(L, 2);
            lua_obj->blockCfg = timeout;
        }
        else
        {
            throw LuaException("invalid block configuration specified");
        }

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring blocking on device: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaDieOnDisconnect - :dod(<enable>)
 *----------------------------------------------------------------------------*/
int DeviceIO::luaDieOnDisconnect(lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        DeviceIO* lua_obj = (DeviceIO*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool enable = getLuaBoolean(L, 2);
        lua_obj->dieOnDisconnect = enable;

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error configuring blocking on device: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}
