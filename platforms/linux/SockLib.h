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

#ifndef __sock_lib__
#define __sock_lib__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

/******************************************************************************
 * SOCKET LIBRARY CLASS
 ******************************************************************************/

class SockLib
{
    public:

        typedef int     (*onPollHandler_t)      (int* flags, void* parm); // configure R/W flags
        typedef int     (*onActiveHandler_t)    (int sock, int flags, void* parm);

        static const int PORT_STR_LEN = 16;
        static const int HOST_STR_LEN = 64;
        static const int SERV_STR_LEN = 64;
        
        static void         initLib             (void); // initializes library
        static void         deinitLib           (void); // de-initializes library
        static void         signalexit          (void); // de-initializes library
        static int          sockstream          (const char* ip_addr, int port, bool is_server, bool* block);
        static int          sockdatagram        (const char* ip_addr, int port, bool is_server, bool* block, const char* multicast_group);
        static int          socksend            (int fd, const void* buf, int size, int timeout);
        static int          sockrecv            (int fd, void* buf, int size, int timeout);
        static int          sockinfo            (int fd, char** local_ipaddr, int* local_port, char** remote_ipaddr, int* remote_port);
        static void         sockclose           (int fd);
        static int          startserver         (const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm);
        static int          startclient         (const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm);
    
    private:
        
        static bool         signal_exit;
        
        static int          sockcreate          (int type, const char* ip_addr, int port, bool is_server, bool* block);
        static int          sockoptions         (int socket_fd, bool reuse, bool tcp);
        static int          sockkeepalive       (int socket_fd, int idle=60, int cnt=12, int intvl=5);
        static int          sockreuse           (int socket_fd);
        static int          socknonblock        (int socket_fd);
        static int          sockmulticast       (int socket_fd, const char* group);
};

#endif  /* __sock_lib__ */
