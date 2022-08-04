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

#include "TcpSocket.h"
#include "StringLib.h"
#include "EventLib.h"

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
        mlog(e.level(), "Error creating TcpSocket: %s", e.what());
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
            mlog(CRITICAL, "Unable to obtain socket information");
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

    if(sock < 0)    return false;
    else            return true;
}

/*----------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
void TcpSocket::closeConnection(void)
{
    if(sock != INVALID_RC)
    {
        mlog(INFO, "closing connection on socket: %s:%d", ip_addr, port);
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
                mlog(INFO, "Exiting tcp connection thread for %s:%d... dying on disconnect.", socket->ip_addr, socket->port);
                break;
            }

            /* Make Connection */
            socket->sock = SockLib::sockstream(socket->ip_addr, socket->port, socket->is_server, &socket->alive);

            /* Handle Connection Outcome */
            if(socket->sock < 0)
            {
                mlog(INFO, "Unable to establish tcp connection to %s:%d... retrying", socket->ip_addr, socket->port);
            }
            else
            {
                mlog(INFO, "Connection established to %s:%d", socket->ip_addr, socket->port);
                connected_once = true;
            }
        }

        LocalLib::performIOTimeout();
    }

    return NULL;
}
