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

#include "OsApi.h"
#include "Uart.h"
#include "LogLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - uart(<device name> <baud> <parity>)
 *
 *    <device name> is the system name of device resource
 *
 *    <parity> is dev.NONE, dev.ODD, dev.EVEN
 *----------------------------------------------------------------------------*/
int Uart::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* dev_name  = getLuaString(L, 1);
        int         baud_rate = (int)getLuaInteger(L, 2);
        int         parity    = (int)getLuaInteger(L, 3);

        /* Return File Device Object */
        return createLuaObject(L, new Uart(L, dev_name, baud_rate, (Uart::parity_t)parity));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating Uart: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Uart::Uart (lua_State* L, const char* _device, int _baud, parity_t _parity):
    DeviceObject(L, DUPLEX)
{
    /* Set Decriptor */
    fd = TTYLib::ttyopen(_device, _baud, _parity);

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s:%d:8%c1", _device, _baud, _parity) + 1;
    config = new char[cfglen];
    sprintf(config, "%s:%d:8%c1", _device, _baud, _parity);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Uart::~Uart (void)
{
    if(config) delete [] config;
    closeConnection();
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool Uart::isConnected (int num_connections)
{
    (void)num_connections;
    return (fd != INVALID_RC);
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void Uart::closeConnection (void)
{
    if(fd != INVALID_RC)
    {
        TTYLib::ttyclose(fd);
        fd = INVALID_RC;
    }
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int Uart::writeBuffer (const void* buf, int len)
{
    if(buf == NULL || len <= 0) return PARM_ERR_RC;
    int ret = TTYLib::ttywrite(fd, buf, len, SYS_TIMEOUT);
    if(ret < 0) closeConnection();
    return ret;

}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Uart::readBuffer (void* buf, int len)
{
    if(buf == NULL || len <= 0) return PARM_ERR_RC;
    int ret = TTYLib::ttyread(fd, buf, len, SYS_TIMEOUT);
    if(ret < 0) closeConnection();
    return ret;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int Uart::getUniqueId (void)
{
    return fd;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* Uart::getConfig (void)
{
    if(config)  return config;
    else        return "null";
}
