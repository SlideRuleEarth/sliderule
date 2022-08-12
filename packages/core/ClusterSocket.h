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

#ifndef __cluster_socket__
#define __cluster_socket__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "Table.h"
#include "OsApi.h"
#include "DeviceObject.h"
#include "TcpSocket.h"

/******************************************************************************
 * CLUSTER SOCKET CLASS
 ******************************************************************************/

class ClusterSocket: public TcpSocket
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const int CONNECTION_TIMEOUT     = 5; // seconds
        static const int INITIAL_POLL_SIZE      = 16;
        static const int METER_PERIOD_MS        = 1000; // milliseconds
        static const int METER_SEND_THRESH      = 128; // 50%
        static const int METER_BUF_SIZE         = 256;
        static const int MSG_HDR_SIZE           = 4;
        static const int MSG_BUFFER_SIZE        = 0x10000; // 64KB
        static const int MIN_BUFFER_SIZE        = 0x0400; // 1KB
        static const int MAX_MSG_SIZE           = 0x10000000; // 256MB
        static const int MAX_NUM_CONNECTIONS    = 256;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef enum {
            QUEUE,
            BUS,
        } protocol_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int      luaCreate           (lua_State* L);

                        ClusterSocket       (lua_State* L, const char* _ip_addr, int _port, role_t _role, protocol_t _protocol, bool _is_server, bool _is_blind=false, const char* passthruq=NULL);
                        ~ClusterSocket      (void);

        bool            isConnected         (int num_connections = 1) override;
        void            closeConnection     (void) override;
        int             writeBuffer         (const void *buf, int len, int timeout=SYS_TIMEOUT) override;
        int             readBuffer          (void *buf, int len, int timeout=SYS_TIMEOUT) override;

    private:

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef struct
        {
            int64_t   prev; // time
            uint8_t*  payload; // dynamically allocated
            int32_t   payload_size; // calculated from header
            int32_t   payload_index; // goes negative for header
            int32_t   buffer_index; // signed for comparisons
            int32_t   buffer_size; // returned from receive call
            uint8_t   buffer[MSG_BUFFER_SIZE];
        } read_connection_t;

        typedef struct
        {
            Subscriber* subconnq;
            Subscriber::msgRef_t payload_ref;
            uint32_t  payload_left;
            uint32_t  bytes_processed;
            uint32_t  buffer_index;
            uint8_t   buffer[MSG_BUFFER_SIZE];
            uint8_t   meter;
        } write_connection_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool                            connecting;
        Thread*                         connector;
        Table<read_connection_t*, int>  read_connections;
        Table<write_connection_t*, int> write_connections;

        protocol_t                      protocol;
        bool                            is_server;
        bool                            is_blind; // send as fast as you can, tolerate drops in data

        const char*                     sockqname;
        Publisher*                      pubsockq;
        Subscriber*                     subsockq;

        bool                            spin_block;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static void*    connectionThread    (void* parm);
        static int      pollHandler         (int fd, short* events, void* parm);
        static int      activeHandler       (int fd, int flags, void* parm);

        int             onRead              (int fd);
        int             onWrite             (int fd);
        int             onAlive             (int fd);
        int             onConnect           (int fd);
        int             onDisconnect        (int fd);
        uint8_t         qMeter              (void);
};

#endif  /* __cluster_socket__ */
