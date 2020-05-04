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

#include "UdpSocket.h"
#include "OsApi.h"
#include "StringLib.h"
#include "LogLib.h"

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
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating UdpSocket: %s\n", e.errmsg);
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

    if(sock == INVALID_RC)    return false;
    else                      return true;
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
