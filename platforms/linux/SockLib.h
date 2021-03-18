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

#ifndef __sock_lib__
#define __sock_lib__

/******************************************************************************
 * SOCKET LIBRARY CLASS
 ******************************************************************************/

class SockLib
{
    public:

        typedef int     (*onPollHandler_t)      (int sock, short* events, void* parm); // configure R/W flags
        typedef int     (*onActiveHandler_t)    (int sock, int flags, void* parm);

        static const char* IPV4_ENV_VAR_NAME;

        static const int IPV4_STR_LEN = 16;
        static const int PORT_STR_LEN = 16;
        static const int HOST_STR_LEN = 64;
        static const int SERV_STR_LEN = 64;

        static void         init                (void); // initializes library
        static void         deinit              (void); // de-initializes library
        static void         signalexit          (void); // de-initializes library
        static int          sockstream          (const char* ip_addr, int port, bool is_server, bool* block);
        static int          sockdatagram        (const char* ip_addr, int port, bool is_server, bool* block, const char* multicast_group);
        static int          socksend            (int fd, const void* buf, int size, int timeout);
        static int          sockrecv            (int fd, void* buf, int size, int timeout);
        static int          sockinfo            (int fd, char** local_ipaddr, int* local_port, char** remote_ipaddr, int* remote_port);
        static void         sockclose           (int fd);
        static int          startserver         (const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, bool* active, void* parm);
        static int          startclient         (const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, bool* active, void* parm);
        static const char*  sockhost            (void);
        static const char*  sockipv4            (void);

    private:

        static bool         signal_exit;
        static char         local_host_name[HOST_STR_LEN];
        static char         ipv4[IPV4_STR_LEN];

        static int          sockcreate          (int type, const char* ip_addr, int port, bool is_server, bool* block);
        static int          sockoptions         (int socket_fd, bool reuse, bool tcp);
        static int          sockkeepalive       (int socket_fd, int idle=60, int cnt=12, int intvl=5);
        static int          sockreuse           (int socket_fd);
        static int          socknonblock        (int socket_fd);
        static int          sockmulticast       (int socket_fd, const char* group);
};

#endif  /* __sock_lib__ */
