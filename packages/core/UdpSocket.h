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

#ifndef __udp_socket__
#define __udp_socket__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"
#include "DeviceObject.h"

/******************************************************************************
 * UDP SOCKET CLASS
 ******************************************************************************/

class UdpSocket: public DeviceObject
{
    public:

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate           (lua_State* L);

                        UdpSocket           (lua_State* L, const char* _ip_addr, int _port, bool _server, const char* _multicast_group);
        virtual         ~UdpSocket          (void);

        bool            isConnected         (int num_connections = 0);
        void            closeConnection     (void);
        int             writeBuffer         (const void* buf, int len);
        int             readBuffer          (void* buf, int len);
        int             getUniqueId         (void);
        const char*     getConfig           (void);

        const char*     getIpAddr           (void);
        int             getPort             (void);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int                 sock;
        char*               ip_addr;
        int                 port;
        char*               config; // <ip_address>:<port>
};

#endif  /* __udp_socket__ */
