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
