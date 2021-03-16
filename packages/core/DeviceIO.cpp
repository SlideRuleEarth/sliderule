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

#include "DeviceIO.h"
#include "Dictionary.h"
#include "StringLib.h"
#include "OsApi.h"
#include "MsgQ.h"
#include "List.h"
#include "EventLib.h"
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
 * luaLogPktStats - :stats([<event level>])
 *----------------------------------------------------------------------------*/
int DeviceIO::luaLogPktStats(lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        DeviceIO* lua_obj = (DeviceIO*)getLuaSelf(L, 1);

        /* Get Event Level */
        event_level_t lvl = (event_level_t)getLuaInteger(L, 2, true, INVALID_EVENT_LEVEL);

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "processed (bytes)", lua_obj->bytesProcessed);
        LuaEngine::setAttrInt(L, "dropped (bytes)", lua_obj->bytesProcessed);
        LuaEngine::setAttrInt(L, "processed (packets)", lua_obj->packetsProcessed);
        LuaEngine::setAttrInt(L, "dropped (packets)", lua_obj->packetsDropped);

        /* Log Stats */
        mlog(lvl, "processed (bytes):   %d", lua_obj->bytesProcessed);
        mlog(lvl, "dropped (bytes):     %d", lua_obj->bytesDropped);
        mlog(lvl, "processed (packets): %d", lua_obj->packetsProcessed);
        mlog(lvl, "dropped (packets):   %d", lua_obj->packetsDropped);

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error logging device I/O statistics: %s", e.what());
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
            throw RunTimeException("device invalid... unable to execute command!");
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
            throw RunTimeException ("timeout occurred waiting for connection on device");
        }

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error waiting on device: %s", e.what());
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
            throw RunTimeException("invalid block configuration specified");
        }

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring blocking on device: %s", e.what());
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
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error configuring blocking on device: %s", e.what());
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}
