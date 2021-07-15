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

#include "UdpSocket.h"
#include "OsApi.h"
#include "StringLib.h"
#include "EventLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - udp(<ip_addr>, <port>, <dev.SERVER|dev.CLIENT>, [<multicast address>])
 *----------------------------------------------------------------------------*/
int UdpSocket::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* ip_addr   = getLuaString(L, 1);
        int         port      = (int)getLuaInteger(L, 2);
        bool        is_server = getLuaBoolean(L, 3);
        const char* multicast = getLuaString(L, 4, true, NULL);

        /* Get Server Parameter */
        if(is_server)
        {
            if(StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*"))
            {
                ip_addr = NULL;
            }
        }

        /* Return File Device Object */
        return createLuaObject(L, new UdpSocket(L, ip_addr, port, is_server, multicast));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating UdpSocket: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UdpSocket::UdpSocket(lua_State* L, const char* _ip_addr, int _port, bool _server, const char* _multicast_group):
    DeviceObject(L, DUPLEX)
{
    /* Initial Socket Parameters */
    sock    = INVALID_RC;
    ip_addr = NULL;
    port    = _port;

    /* Set IP Address */
    if(_multicast_group)
    {
        ip_addr = StringLib::duplicate(_multicast_group);
    }
    else if(_ip_addr)
    {
        ip_addr = StringLib::duplicate(_ip_addr);
    }

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port) + 1;
    config = new char[cfglen];
    sprintf(config, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port);

    /* Create UDP Socket */
    sock = SockLib::sockdatagram(ip_addr, port, _server, NULL, _multicast_group);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
UdpSocket::~UdpSocket(void)
{
    closeConnection();
    if(ip_addr) delete [] ip_addr;
    if(config) delete [] config;
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool UdpSocket::isConnected(int num_connections)
{
    (void)num_connections;

    if(sock < 0)    return false;
    else            return true;
}

/*----------------------------------------------------------------------------
 * closeConnection
 *----------------------------------------------------------------------------*/
void UdpSocket::closeConnection(void)
{
    SockLib::sockclose(sock);
    sock = INVALID_RC;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *----------------------------------------------------------------------------*/
int UdpSocket::writeBuffer(const void* buf, int len)
{
    if(buf == NULL || len <= 0) return PARM_ERR_RC;
    return SockLib::socksend(sock, buf, len, SYS_TIMEOUT);
}

/*----------------------------------------------------------------------------
 * readBuffer
 *----------------------------------------------------------------------------*/
int UdpSocket::readBuffer(void* buf, int len)
{
    if(buf == NULL || len <= 0) return PARM_ERR_RC;
    return SockLib::sockrecv(sock, buf, len, SYS_TIMEOUT);
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int UdpSocket::getUniqueId (void)
{
    return sock;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* UdpSocket::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* UdpSocket::getIpAddr (void)
{
    return ip_addr;
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int UdpSocket::getPort (void)
{
    return port;
}
