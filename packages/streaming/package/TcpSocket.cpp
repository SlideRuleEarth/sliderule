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
        const int   port      = (int)getLuaInteger(L, 2);
        const bool  is_server = getLuaBoolean(L, 3);
        const bool  die       = getLuaBoolean(L, 4, true, false);

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
TcpSocket::TcpSocket(lua_State* L, const char* _ip_addr, int _port, bool _server, const bool* block, bool _die_on_disconnect):
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
    const int cfglen = snprintf(NULL, 0, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port) + 1;
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
        if(sock >= 0) mlog(DEBUG, "Connection [%d] established to %s:%d", sock, ip_addr, port);
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
    const int cfglen = snprintf(NULL, 0, "%s:%d", ip_addr == NULL ? "0.0.0.0" : ip_addr, port) + 1;
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
    delete connector;
    TcpSocket::closeConnection();
    delete [] ip_addr;
    delete [] config;
}

/*----------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
bool TcpSocket::isConnected(int num_connections)
{
    (void)num_connections;
    return (sock >= 0);
}

/*----------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
void TcpSocket::closeConnection(void)
{
    if(sock != INVALID_RC)
    {
        mlog(DEBUG, "Closing connection on socket: %s:%d", ip_addr, port);
        SockLib::sockclose(sock);
        sock = INVALID_RC;
    }
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *
 *  returns when full buffer written out
 *----------------------------------------------------------------------------*/
int TcpSocket::writeBuffer(const void* buf, int len, int timeout)
{
    const unsigned char* cbuf = reinterpret_cast<const unsigned char*>(buf);

    /* Check Parameters */
    if(buf == NULL || len <= 0) return PARM_ERR_RC;

    /* Timeout If Not Connected*/
    if(!isConnected())
    {
        OsApi::performIOTimeout();
        return TIMEOUT_RC;
    }

    /* Send Data */
    int c = 0;
    while(c < len && alive)
    {
        const int ret = SockLib::socksend(sock, &cbuf[c], len - c, timeout);
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
int TcpSocket::readBuffer(void* buf, int len, int timeout)
{
    /* Check Parameters */
    if(buf == NULL || len <= 0) return PARM_ERR_RC;

    /* Timeout If Not Connected*/
    if(!isConnected())
    {
        OsApi::performIOTimeout();
        return TIMEOUT_RC;
    }

    /* Receive Data */
    const int ret = SockLib::sockrecv(sock, buf, len, timeout);
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
const char* TcpSocket::getIpAddr (void) const
{
    return ip_addr;
}

/*----------------------------------------------------------------------------
 * getPort
 *----------------------------------------------------------------------------*/
int TcpSocket::getPort (void) const
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
    TcpSocket* socket = static_cast<TcpSocket*>(parm);
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

        OsApi::performIOTimeout();
    }

    return NULL;
}
