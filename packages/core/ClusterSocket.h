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

#ifndef __cluster_socket__
#define __cluster_socket__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "Ordering.h"
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
        static const int IP_ADDR_STR_SIZE       = 64;
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

        bool            isConnected         (int num_connections = 1);
        void            closeConnection     (void);
        int             writeBuffer         (const void *buf, int len);
        int             readBuffer          (void *buf, int len);

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
        Ordering<read_connection_t*>    read_connections;
        Ordering<write_connection_t*>   write_connections;

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
        static int      pollHandler         (int* flags, void* parm);
        static int      activeHandler       (int fd, int flags, void* parm);

        int             onRead              (int fd);
        int             onWrite             (int fd);
        int             onAlive             (int fd);
        int             onConnect           (int fd);
        int             onDisconnect        (int fd);
        uint8_t         qMeter              (void);
};

#endif  /* __cluster_socket__ */
