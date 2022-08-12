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

        virtual bool        isConnected         (int num_connections = 1) override;
        virtual void        closeConnection     (void) override;
        virtual int         writeBuffer         (const void* buf, int len, int timeout=SYS_TIMEOUT) override;
        virtual int         readBuffer          (void* buf, int len, int timeout=SYS_TIMEOUT) override;
        virtual int         getUniqueId         (void) override;
        virtual const char* getConfig           (void) override;

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
