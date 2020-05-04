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

#ifndef __tcp_socket__
#define __tcp_socket__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "DeviceObject.h"

/******************************************************************************
 * TCP SOCKET CLASS
 ******************************************************************************/

class TcpSocket: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int          luaCreate           (lua_State* L);

                            TcpSocket           (lua_State* L, const char* _ip_addr, int _port, bool _server, bool* block, bool _die_on_disconnect);
                            TcpSocket           (lua_State* L, int _sock, const char* _ip_addr=NULL, int _port=0, role_t _role=DUPLEX);
        virtual             ~TcpSocket          (void);

        virtual bool        isConnected         (int num_connections = 1);
        virtual void        closeConnection     (void);
        virtual int         writeBuffer         (const void* buf, int len);
        virtual int         readBuffer          (void* buf, int len);
        virtual int         getUniqueId         (void);
        virtual const char* getConfig           (void);

        const char*         getIpAddr           (void);
        int                 getPort             (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int         sock;
        char*       ip_addr;
        int         port;
        char*       config; // <ip_address>:<port>
        bool        alive;
        Thread*     connector;
        bool        is_server;
        bool        die_on_disconnect;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    connectionThread    (void* parm);
};

#endif  /* __tcp_socket__ */
