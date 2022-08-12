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

#include "OsApi.h"
#include "Uart.h"
#include "EventLib.h"

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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating Uart: %s", e.what());
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
int Uart::writeBuffer (const void* buf, int len, int timeout)
{
    if(buf == NULL || len <= 0) return PARM_ERR_RC;
    int ret = TTYLib::ttywrite(fd, buf, len, timeout);
    if(ret < 0) closeConnection();
    return ret;

}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int Uart::readBuffer (void* buf, int len, int timeout)
{
    if(buf == NULL || len <= 0) return PARM_ERR_RC;
    int ret = TTYLib::ttyread(fd, buf, len, timeout);
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
