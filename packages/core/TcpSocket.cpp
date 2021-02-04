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

#include "TcpSocket.h"
#include "StringLib.h"
#include "LogLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - tcp(<ip_addr>, <port>, <dev.SERVER|dev.CLIENT>, [<die on disconnect>])
 *----------------------------------------------------------------------------*/
int TcpSocket::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* ip_addr   = getLuaString(L, 1);
        int         port      = (int)getLuaInteger(L, 2);
        bool        is_server = getLuaBoolean(L, 3);
        bool        die       = getLuaBoolean(L, 4, true, false);

        /* Get Server Parameter */
        if(is_server)
        {
            if(StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*"))
            {
                ip_addr = NULL;
            }
        }

        /* Return File Device Object */
        return createLuaObject(L, new TcpSocket(L, ip_addr, port, is_server, NULL, die));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating TcpSocket: %s\n", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
TcpSocket::TcpSocket(lua_State* L, const char* _ip_addr, int _port, bool _server, bool* block, bool _die_on_disconnect):
    DeviceObject(L, DUPLEX)
{
    /* Initial Socket Parameters */
    sock    = INVALID_RC;
    ip_addr = NULL;
    port    = _port;

    /* Set IP Address */
    if(_ip_addr)
    {
        ip_addr = StringLib::duplicate(_ip_addr);
    }

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port) + 1;
    config = new char[cfglen];
    sprintf(config, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port);

    /* Set Server Options */
    is_server = _server;
    die_on_disconnect = _die_on_disconnect;
    alive = true;

    /* Start Connection */
    if(block)
    {
        connector = NULL;
        sock = SockLib::sockstream(ip_addr, port, is_server, block);
    }
    else
    {
        connector = new Thread(connectionThread, this);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 * 
 *  special constructor that stores ip address and uses passed in socket
 *----------------------------------------------------------------------------*/
TcpSocket::TcpSocket(lua_State* L, int _sock, const char* _ip_addr, int _port, role_t _role):
    DeviceObject(L, _role)
{
    /* Initial Parameters */
    sock = _sock;

    /* Get Socket Info */
    if(_ip_addr == NULL)
    {
        if(SockLib::sockinfo(sock, NULL, &port, &ip_addr, NULL) < 0)
        {
            mlog(CRITICAL, "Unable to obtain socket information\n");
            ip_addr = NULL;
            port = -1;
        }
    }
    else
    {
        ip_addr = StringLib::duplicate(_ip_addr);
        port = _port;
    }

    /* Set Configuration */
    int cfglen = snprintf(NULL, 0, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port) + 1;
    config = new char[cfglen];
    sprintf(config, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port);

    /* Set Server Options */
    is_server = false;
    die_on_disconnect = false;
    alive = true;

    /* Set Connection Parameters */
    connector = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
TcpSocket::~TcpSocket(void)
{
    /* Kill Listener... so it doesn't automatically reconnect */
    alive = false;
    if(connector) delete connector;
    closeConnection();
    if(ip_addr) delete [] ip_addr;
    if(config) delete [] config;
}

/*----------------------------------------------------------------------------
 * 
 *----------------------------------------------------------------------------*/
bool TcpSocket::isConnected(int num_connections)
{
    (void)num_connections;

    if(sock == INVALID_RC)  return false;
    else                    return true;
}

/*----------------------------------------------------------------------------
 * 
 *----------------------------------------------------------------------------*/
void TcpSocket::closeConnection(void)
{
    if(sock != INVALID_RC)
    {
        mlog(CRITICAL, "closing connection on socket: %s %d\n", ip_addr, port);
        SockLib::sockclose(sock);
        sock = INVALID_RC;
    }
}

/*----------------------------------------------------------------------------
 * writeBuffer
 * 
 *  returns when full buffer written out
 *----------------------------------------------------------------------------*/
int TcpSocket::writeBuffer(const void* buf, int len)
{
    unsigned char* cbuf = (unsigned char*)buf;

    /* Check Parameters */
    if(buf == NULL || len <= 0) return PARM_ERR_RC;

    /* Timeout If Not Connected*/
    if(!isConnected())
    {
        LocalLib::performIOTimeout();
        return TIMEOUT_RC;
    }

    /* Send Data */
    int c = 0;
    while(c < len && alive)
    {
        int ret = SockLib::socksend(sock, &cbuf[c], len - c, SYS_TIMEOUT);
        if(ret > 0)
        {
            c += ret;
        }
        else if(ret < 0)
        {
            closeConnection();
            break;
        }
    }

    /* Return Bytes Sent */
    return c;
}

/*----------------------------------------------------------------------------
 * readBuffer
 * 
 *  returns if any data read into buffer
 *----------------------------------------------------------------------------*/
int TcpSocket::readBuffer(void* buf, int len)
{
    /* Check Parameters */
    if(buf == NULL || len <= 0) return PARM_ERR_RC;

    /* Timeout If Not Connected*/
    if(!isConnected())
    {
        LocalLib::performIOTimeout();
        return TIMEOUT_RC;
    }

    /* Receive Data */
    int ret = SockLib::sockrecv(sock, buf, len, SYS_TIMEOUT);
    if(ret < 0)
    {
        closeConnection();
    }

    /* Return Bytes Received */
    return ret;
}

/*----------------------------------------------------------------------------
 * getUniqueId
 *----------------------------------------------------------------------------*/
int TcpSocket::getUniqueId (void)
{
    return sock;
}

/*----------------------------------------------------------------------------
 * getConfig
 *----------------------------------------------------------------------------*/
const char* TcpSocket::getConfig (void)
{
    return config;
}

/*----------------------------------------------------------------------------
 * getIpAddr
 *----------------------------------------------------------------------------*/
const char* TcpSocket::getIpAddr (void)
{
    return ip_addr;
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int TcpSocket::getPort (void)
{
    return port;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * connectionThread
 *----------------------------------------------------------------------------*/
void* TcpSocket::connectionThread(void* parm)
{
    TcpSocket* socket = (TcpSocket*)parm;
    socket->sock = INVALID_RC;
    bool connected_once = false;

    while(socket->alive)
    {
        if(socket->sock < 0)
        {
            /* Check Die On Disconnect */
            if(connected_once && socket->die_on_disconnect)
            {
                mlog(CRITICAL, "Exiting tcp connection thread for %s:%d... dying on disconnect.\n", socket->ip_addr, socket->port);
                break;
            }

            /* Make Connection */
            socket->sock = SockLib::sockstream(socket->ip_addr, socket->port, socket->is_server, &socket->alive);

            /* Handle Connection Outcome */
            if(socket->sock < 0)
            {
                mlog(CRITICAL, "Unable to establish tcp connection to %s:%d... retrying\n", socket->ip_addr, socket->port);
            }
            else
            {
                mlog(CRITICAL, "Connection established to %s:%d\n", socket->ip_addr, socket->port);
                connected_once = true;
            }
        }

        LocalLib::performIOTimeout();
    }

    return NULL;
}
