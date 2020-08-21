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

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "OsApi.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define IPV4_MULTICAST_START    0xE0000000
#define IPV4_MULTICAST_STOP     0xF0000000

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef struct sockaddr_in  socket_address_t;
typedef struct sockaddr     client_address_t;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

bool SockLib::signal_exit = false;

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void SockLib::init()
{
}

/*----------------------------------------------------------------------------
 * deinit
 *----------------------------------------------------------------------------*/
void SockLib::deinit(void)
{
    signalexit();
}

/*----------------------------------------------------------------------------
 * signalExit
 *----------------------------------------------------------------------------*/
void SockLib::signalexit(void)
{
    signal_exit = true;
}

/*----------------------------------------------------------------------------
 * sockstream
 *----------------------------------------------------------------------------*/
int SockLib::sockstream(const char* ip_addr, int port, bool is_server, bool* block)
{
    /* Create initial socket */
    int sock = sockcreate(SOCK_STREAM, ip_addr, port, is_server, block);
    if(sock == INVALID_RC) return INVALID_RC;
    
    if(!is_server) // client
    {
        return sock;
    }
    else // server
    {
        int listen_socket = sock;
        int server_socket = INVALID_RC;

        /* Make Socket a Listen Socket */
        if(listen(listen_socket, 1) != 0)
        {
            dlog("Failed to mark socket bound to %s:%d as a listen socket, %s\n", ip_addr ? ip_addr : "0.0.0.0", port, strerror(errno));
            close(listen_socket);
            return INVALID_RC;
        }

        do {
            /* Build Poll Structure */
            struct pollfd polllist[1];
            polllist[0].fd = listen_socket;
            polllist[0].events = POLLIN;
            polllist[0].revents = 0;

            /* Poll Listener */
            int activity = 0;
            do activity = poll(polllist, 1, SYS_TIMEOUT);
            while(activity == -1 && (errno == EINTR || errno == EAGAIN));

            /* Accept Connection */
            if((activity > 0) && (polllist[0].revents & POLLIN))
            {
                server_socket = accept(listen_socket, NULL, NULL);
            }
        } while(server_socket == INVALID_RC && block && *block && !signal_exit);

        /* Shutdown Listen Socket */
        shutdown(listen_socket, SHUT_RDWR);
        close(listen_socket);

        /* Check Acceptance */
        if(server_socket < 0)
        {
            dlog("Failed to accept connection on %s:%d, %s\n", ip_addr ? ip_addr : "0.0.0.0", port, strerror(errno));
            return INVALID_RC;
        }

        /* Set Keep Alive on Socket */
        if(sockkeepalive(server_socket) < 0)
        {
            sockclose(server_socket);
            return INVALID_RC;
        }

        /* Make Socket Non-Blocking */
        if(socknonblock(server_socket) < 0)
        {
            sockclose(server_socket);
            return INVALID_RC;
        }

        /* Return Server Socket */
        return server_socket;
    }
}

/*----------------------------------------------------------------------------
 * sockdatagram
 *----------------------------------------------------------------------------*/
int SockLib::sockdatagram(const char* ip_addr, int port, bool is_server, bool* block, const char* multicast_group)
{
    /* Create initial socket */
    int sock = sockcreate(SOCK_DGRAM, ip_addr, port, is_server, block);
    if(sock == INVALID_RC) return INVALID_RC;
    
    /* Set Options for Multicast  - IPv4 only */
    if(multicast_group)
    {
        struct in_addr ipv4_addr;
        if(inet_pton(AF_INET, multicast_group, &ipv4_addr) == 1)
        {
            uint32_t naddr = htonl(ipv4_addr.s_addr);
            if(naddr >= IPV4_MULTICAST_START && naddr < IPV4_MULTICAST_STOP)
            {
                if(sockmulticast(sock, multicast_group) < 0)
                {
                    sockclose(sock);
                    return INVALID_RC;
                }
                else
                {
                    dlog("Configured socket on %s:%d to receive multicast packets on %s\n", ip_addr, port, multicast_group);
                }
            }
            else
            {
                dlog("Invalid multicast group address %s - %08X\n", multicast_group, naddr  );
            }
        }
        else
        {
            dlog("Currently only IPv4 group addresses supported: %s\n", multicast_group);
        }
    }
    
    /* Return Socket */
    return sock;
}

/*----------------------------------------------------------------------------
 * socksend
 *
 * Notes: returns number of bytes written after SYS_TIMEOUT duration of trying
 *----------------------------------------------------------------------------*/
int SockLib::socksend(int fd, const void* buf, int size, int timeout)
{
    int activity = 1;
    int revents = POLLOUT;
    int c = TIMEOUT_RC;

    /* Check Sock */
    if(fd == INVALID_RC)
    {
        if(timeout != IO_CHECK) LocalLib::performIOTimeout();
        return TIMEOUT_RC;
    }

    if(timeout != IO_CHECK)
    {
        /* Build Poll Structure */
        struct pollfd polllist[1];
        polllist[0].fd = fd;
        polllist[0].events = POLLOUT | POLLHUP;
        polllist[0].revents = 0;

        /* Poll */
        do activity = poll(polllist, 1, timeout);
        while(activity == -1 && (errno == EINTR || errno == EAGAIN));

        /* Set Activity */
        if(activity > 0)    revents = polllist[0].revents;
        else                revents = 0;
    }

    /* Perform Send */
    if(revents & POLLHUP)
    {
        c = SHUTDOWN_RC;
    }
    else if(revents & POLLOUT)
    {
        c = send(fd, buf, size, MSG_DONTWAIT | MSG_NOSIGNAL);
        if(c == 0)
        {
            c = SHUTDOWN_RC;
        }        
        else if(timeout != IO_CHECK && c < 0)
        {
            dlog("Failed (%d) to send data to ready socket [%0X]: %s\n", c, revents, strerror(errno));
            c = SOCK_ERR_RC;
        }
    }

    /* Return Results */
    return c;
}

/*----------------------------------------------------------------------------
 * sockrecv
 *----------------------------------------------------------------------------*/
int SockLib::sockrecv(int fd, void* buf, int size, int timeout)
{
    int c = TIMEOUT_RC;
    int revents = POLLIN;

    if(timeout != IO_CHECK)
    {
        /* Build Poll Structure */
        struct pollfd polllist[1];
        polllist[0].fd = fd;
        polllist[0].events = POLLIN | POLLHUP;
        polllist[0].revents = 0;

        /* Poll */
        int activity = 1;
        do activity = poll(polllist, 1, timeout);
        while(activity == -1 && (errno == EINTR || errno == EAGAIN));

        /* Set Activity */
        revents = polllist[0].revents;
    }

    /* Perform Receive */
    if(revents & POLLIN)
    {
        c = recv(fd, buf, size, MSG_DONTWAIT);
        if(c == 0)
        {
            c = SHUTDOWN_RC;
        }
        else if(timeout != IO_CHECK && c < 0)
        {
            dlog("Failed (%d) to receive data from ready socket [%0X]: %s\n", c, revents, strerror(errno));
            c = SOCK_ERR_RC;
        }
    }
    else if(revents & POLLHUP)
    {
        c = SHUTDOWN_RC;
    }

    /* Return Results */
    return c;
}

/*----------------------------------------------------------------------------
 * sockinfo
 * 
 * Note: only IPv4 currently supported by this function
 *----------------------------------------------------------------------------*/
int SockLib::sockinfo(int fd, char** local_ipaddr, int* local_port, char** remote_ipaddr, int* remote_port)
{
    socket_address_t local_addr;
    socket_address_t remote_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    /* Get Socket Info */
    if(getsockname(fd, (struct sockaddr *)&local_addr, &addr_size) < 0) return -1;
    if(getpeername(fd, (struct sockaddr *)&remote_addr, &addr_size) < 0) return -1;

    /* Populate Local IP Address */
    if(local_ipaddr)
    {
        *local_ipaddr = new char[16];
        strcpy(*local_ipaddr, inet_ntoa(local_addr.sin_addr));
    }

    /* Populate Local Port */
    if(local_port)
    {
        *local_port = ntohs(local_addr.sin_port);
    }

    /* Populate Remote IP Address */
    if(remote_ipaddr)
    {
        *remote_ipaddr = new char[16];
        strcpy(*remote_ipaddr, inet_ntoa(remote_addr.sin_addr));
    }

    /* Populate Remote Port */
    if(remote_port)
    {
        *remote_port = ntohs(remote_addr.sin_port);
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * sockclose
 *----------------------------------------------------------------------------*/
void SockLib::sockclose(int fd)
{
    shutdown(fd, SHUT_RDWR);
    close(fd);
}

/*----------------------------------------------------------------------------
 * startserver
 *----------------------------------------------------------------------------*/
int SockLib::startserver(const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm)
{
    /* Initialize Connection Variables */
    int flags = 0;
    int prev_flags = 0;
    int pollevents = POLLHUP;
    int num_sockets = 0;
    int max_num_sockets = max_num_connections + 1; // add in listener
    const int INITIAL_POLL_SIZE = 8;
    int pollsize = INITIAL_POLL_SIZE;
    struct pollfd* polllist= new struct pollfd[pollsize];

    /* Create Listen Socket */
    int listen_socket = sockcreate(SOCK_STREAM, ip_addr, port, true, NULL);
    if(listen_socket == INVALID_RC)
    {
        dlog("Unable to establish socket server on %s:%d, failed to create listen socket\n", ip_addr ? ip_addr : "0.0.0.0", port);
        delete [] polllist;
        return INVALID_RC;
    }
    else
    {
        /* Make Socket a Listen Socket */
        if(listen(listen_socket, 1) != 0)
        {
            dlog("Failed to mark socket bound to %s:%d as a listen socket, %s\n", ip_addr ? ip_addr : "0.0.0.0", port, strerror(errno));
            close(listen_socket);
            delete [] polllist;
            return INVALID_RC;
        }

        /* Set Listener in Poll List */
        dlog("Established listener socket on %s:%d\n", ip_addr ? ip_addr : "0.0.0.0", port);
        num_sockets++; // for listener socket
        polllist[0].fd = listen_socket;
        polllist[0].events = POLLIN;
        polllist[0].revents = 0;

        /* Loop While Connecting */
        while(on_poll(&flags, parm) >= 0)
        {
            /* Build Poll Flags */
            if(flags != prev_flags)
            {
                prev_flags = flags;
                pollevents = POLLHUP;
                if(flags & IO_READ_FLAG) pollevents |= POLLIN;
                if(flags & IO_WRITE_FLAG) pollevents |= POLLOUT;
                for(int j = 1; j < num_sockets; j++) polllist[j].events = pollevents;
            }

            /* Poll On All Connections */
            int activity = 0;
            do activity = poll(polllist, num_sockets, 100); // 10Hz
            while(activity == -1 && (errno == EINTR || errno == EAGAIN));

            /* Handle Existing Connections  */
            int i = 1; // skip listener socket
            while(i < num_sockets)
            {
                bool valid_fd = true;
                int cb_stat = 0;
                
                /* Handle Errors */
                if(polllist[i].revents & POLLERR)
                {
                    dlog("Poll error detected on server socket [%d]: %s\n", polllist[i].fd, strerror(errno));
                    cb_stat = -1; // treat like a callback error or disconnect
                }
                else if(polllist[i].revents & POLLNVAL)
                {
                    dlog("Socket [%d] not open, yet trying to poll: %s\n", polllist[i].fd, strerror(errno));                    
                    valid_fd = false;
                }
                else
                {
                    /* Call Back for Active Connection */
                    int actevents = IO_ALIVE_FLAG;
                    if(polllist[i].revents & POLLIN) actevents |= IO_READ_FLAG;
                    if(polllist[i].revents & POLLOUT) actevents |= IO_WRITE_FLAG;
                    cb_stat = on_act(polllist[i].fd, actevents, parm);
                }
                
                /* Handle Disconnections */
                if( (cb_stat < 0) || (polllist[i].revents & POLLHUP) )
                {       
                    /* Call Back for Disconnect */
                    on_act(polllist[i].fd, IO_DISCONNECT_FLAG, parm);
                    sockclose(polllist[i].fd);
                    valid_fd = false;
                }
                
                /* Check if FD still Valid */
                if(!valid_fd)
                {
                    /* Remove from Polling */
                    dlog("Disconnected [%d] from server socket %s:%d\n", polllist[i].fd, ip_addr ? ip_addr : "0.0.0.0", port);                               
                    int connections_left = num_sockets - 1;
                    if(i < connections_left)
                    {
                        LocalLib::move(&polllist[i], &polllist[i+1], (connections_left - i) * sizeof(struct pollfd));
                    }

                    /* Decrement Number of Connections */
                    num_sockets = connections_left;                
                }
                else
                {   
                    /* Clear Return Events */
                    polllist[i].revents = 0;                
                    
                    // if there was a disconnect, then the 'fd' list
                    // moved down and 'i' does not need to be incremented
                    // but if it remained connected then 'i' increments
                    i++;
                }
            }

            /* Handle New Connections */
            if(polllist[0].revents & (POLLNVAL | POLLERR))
            {
                dlog("Error detected on listener socket: %s\n", strerror(errno));
            }
            else if(polllist[0].revents & POLLIN)
            {
                if(max_num_sockets <= 0 || max_num_sockets > num_sockets)
                {
                    /* Accept Next Connection */
                    client_address_t    client_address;
                    socklen_t           address_length = sizeof(socket_address_t);

                    int client_socket = accept(listen_socket, &client_address, &address_length);
                    if(client_socket != -1)
                    {
                        /* Set Non-Blocking */
                        if(socknonblock(client_socket) == 0)
                        {
                            /* Call On Acceptance Call-Back */
                            if(on_act(client_socket, IO_CONNECT_FLAG, parm) >= 0)
                            {
                                dlog("Established connection [%d] to server socket %s:%d\n", client_socket, ip_addr ? ip_addr : "0.0.0.0", port);

                                /* check for need to resize and preserve indices */
                                if(num_sockets >= pollsize)
                                {
                                    /* new poll size */
                                    int new_pollsize = pollsize * 2;

                                    /* allocate new structures */
                                    struct pollfd* new_polllist = new struct pollfd[new_pollsize];

                                    /* copy old structures into new structures */
                                    LocalLib::copy(new_polllist, polllist, sizeof(struct pollfd) * pollsize);

                                    /* delete old structures */
                                    delete [] polllist;

                                    /* reset pointers to new structures */
                                    polllist = new_polllist;
                                    pollsize = new_pollsize;
                                }

                                /* Populate New Connection */
                                polllist[num_sockets].fd = client_socket;
                                polllist[num_sockets].events = pollevents;
                                polllist[num_sockets].revents = 0;

                                /* Increment Number of Connections */
                                num_sockets++;
                            }
                            else
                            {
                                SockLib::sockclose(client_socket);
                            }
                        }
                        else
                        {
                            dlog("Failed to set socket to non-blocking %s:%d\n", ip_addr ? ip_addr : "0.0.0.0", port);
                        }
                    }
                    else
                    {
                        dlog("Failed to set accept connection on %s:%d\n", ip_addr ? ip_addr : "0.0.0.0", port);
                    }
                }
                else
                {
                    dlog("Maximum number of sockets exceeded: %d\n", max_num_sockets);
                }
            }
        }
    }
    
    /* Close Listening Socket */
    sockclose(listen_socket);

    /* Disconnect Existing Connections */
    for(int i = 1; i < num_sockets; i++)
    {
        on_act(polllist[i].fd, IO_DISCONNECT_FLAG, parm);
        sockclose(polllist[i].fd);
    }
    
    /* Clean Up Allocated Memory */
    delete [] polllist;
    
    /* Return Success */
    return 0;
}

/*----------------------------------------------------------------------------
 * startclient
 *----------------------------------------------------------------------------*/
int SockLib::startclient(const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, void* parm)
{
    /* Initialize Connection Variables */
    int flags = 0;
    bool connected = false;
    int max_num_sockets = max_num_connections + 1; // for listener socket
    int num_sockets = 0;
    struct pollfd polllist[1];

    /* Loop While Connecting */
    while(on_poll(&flags, parm) >= 0)
    {
        /* If Not Connected */
        if(!connected)
        {
            /* Check for Connection Upper Limit */
            if(max_num_sockets > 0 && num_sockets >= max_num_sockets)
            {
                dlog("Maximum number of connections reached: %d\n", max_num_connections);
                return -1;
            }

            /* Connect Client Socket */
            int client_socket = sockstream(ip_addr, port, false, NULL);
            if(client_socket >= 0)
            {
                /* Call Connection Callback */
                if(on_act(client_socket, IO_CONNECT_FLAG, parm) < 0)
                {
                    dlog("Callback on connection returned error, exiting\n");
                    sockclose(client_socket);
                    return -1;
                }

                /* Update Connection Variables */
                dlog("Client socket [%d] connection made to %s:%d\n", client_socket, ip_addr ? ip_addr : "0.0.0.0", port);
                polllist[0].fd = client_socket;
                connected = true;
                num_sockets++;
            }
            else // error connected
            {
                if(client_socket != WOULDBLOCK_RC)
                {
                    dlog("Unable to create client socket on %s:%d\n", ip_addr ? ip_addr : "0.0.0.0", port);
                    return -1;
                }
                else // timeout
                {
                    LocalLib::performIOTimeout();
                }
            }
        }
        
        /* If Connected */
        if(connected)
        {
            /* Build Poll Flags */    
            int pollevents = POLLHUP;
            if(flags & IO_READ_FLAG) pollevents |= POLLIN;
            if(flags & IO_WRITE_FLAG) pollevents |= POLLOUT;

            /* Initialize Polling Structure */
            polllist[0].events = pollevents;
            polllist[0].revents = 0;

            /* Perform Poll */
            int activity = 0;
            do activity = poll(polllist, 1, SYS_TIMEOUT);
            while(activity == -1 && (errno == EINTR || errno == EAGAIN));

            /* Handle Errors */
            bool valid_fd = true;
            int cb_stat = 0;
            if(polllist[0].revents & POLLERR)
            {
                dlog("Poll error detected on server socket [%d]: %s\n", polllist[0].fd, strerror(errno));
                cb_stat = -1; // treat like a callback error or disconnect
            }
            else if(polllist[0].revents & POLLNVAL)
            {
                dlog("Socket [%d] not open, yet trying to poll: %s\n", polllist[0].fd, strerror(errno));                    
                valid_fd = false;
            }
            else
            {
                /* Call Back for Active Connection */
                int actevents = IO_ALIVE_FLAG;
                if(polllist[0].revents & POLLIN) actevents |= IO_READ_FLAG;
                if(polllist[0].revents & POLLOUT) actevents |= IO_WRITE_FLAG;
                cb_stat = on_act(polllist[0].fd, actevents, parm);
            }

            /* Handle Disconnects */
            if( (cb_stat < 0) || (polllist[0].revents & POLLHUP) )
            {                
                on_act(polllist[0].fd, IO_DISCONNECT_FLAG, parm); // no sense checking error, nothing to do
                dlog("Disconnect [%d] from client socket %s:%d\n", polllist[0].fd, ip_addr ? ip_addr : "0.0.0.0", port);                               
                SockLib::sockclose(polllist[0].fd);
                valid_fd = false;
            }

            /* Check if FD still Valid */
            if(!valid_fd)
            {
                connected = false;
            }
        }
    }
    
    return 0;
}

/*----------------------------------------------------------------------------
 * sockcreate
 *----------------------------------------------------------------------------*/
int SockLib::sockcreate(int type, const char* ip_addr, int port, bool is_server, bool* block)
{
    struct addrinfo     hints, *result, *rp;
    int                 sock = INVALID_RC;
    int                 status = -1; // start with error condition
    char                portstr[PORT_STR_LEN];
    char                host[HOST_STR_LEN];
    char                serv[SERV_STR_LEN];
    
    /* Check Address */
    if(!ip_addr) ip_addr = "0.0.0.0"; // wildcard
    
    /* Initialize Port */
    snprintf(portstr, PORT_STR_LEN, "%d", port);
    
    /* Get Destination Host */
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* IPv4 or IPv6 */
    hints.ai_socktype = type;
    status = getaddrinfo(ip_addr, portstr, &hints, &result);
    if(status != 0) 
    {
        dlog("Failed to get address info for %s:%d, %s\n", ip_addr, port, gai_strerror(status));
        return TCP_ERR_RC;
    }

    /* Try each address until we successfully connect. */
    for(rp = result; rp != NULL; rp = rp->ai_next) 
    {
        /* Get address information */
        getnameinfo(rp->ai_addr,rp->ai_addrlen, host, HOST_STR_LEN, serv, SERV_STR_LEN, NI_NUMERICHOST | NI_NUMERICSERV);
        
        /* Create socket */
        sock = socket(rp->ai_family, rp->ai_socktype, 0);
        if(sock < 0)
        {
            dlog("Failed to open socket for %s:%s, %s\n", host, serv, strerror(errno));
            continue;
        }

        if(is_server)
        {
            /* Set Reuse Option on Listen Socket */
            if(sockreuse(sock) < 0)
            {
                close(sock);
                continue;
            }

            /* Bind Socket */
            status = bind(sock, rp->ai_addr, rp->ai_addrlen);
            if(status < 0)  
            {
                dlog("Failed to bind socket to %s:%s, %s\n", host, serv, strerror(errno));
                close(sock);
            }
            else            
            {
                break;
            }
        }
        else
        {
            do {
                /* Connect Socket */
                status = connect(sock, rp->ai_addr, rp->ai_addrlen);
                if(status < 0) 
                {
                    dlog("Failed to connect socket to %s:%s... %s\n", host, serv, strerror(errno));
                    LocalLib::performIOTimeout();
                }
            } while(status < 0 && block && *block && !signal_exit);
            
            /* Check Connection */
            if(status < 0)  close(sock);
            else            break;
        }
    }

    /* Free linked list returned by getaddrinfo */
    freeaddrinfo(result);

    /* Check success of connection */
    if (rp == NULL) 
    {
        dlog("Failed to create socket for %s:%d\n", ip_addr, port);
        return WOULDBLOCK_RC;
    }

    /* Set Keep Alive on Socket */
    if(type == SOCK_STREAM)
    {
        if(sockkeepalive(sock) < 0)
        {
            sockclose(sock);
            return TCP_ERR_RC;
        }

        /* Make Socket Non-Blocking*/
        if(socknonblock(sock) < 0)
        {
            sockclose(sock);
            return TCP_ERR_RC;
        }
    }

    /* Return Client TcpSocket */
    return sock;    
}

/*----------------------------------------------------------------------------
 * sockkeepalive
 *----------------------------------------------------------------------------*/
int SockLib::sockkeepalive(int socket_fd, int idle, int cnt, int intvl)
{
    int         optval;
    socklen_t   optlen = sizeof(optval);

    optval = 1; // set SO_KEEPALIVE on a socket to true (1)
    if (setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        dlog("Failed to set SO_KEEPALIVE option on socket, %s\n", strerror(errno));
        return TCP_ERR_RC;
    }

    optval = idle; // set TCP_KEEPIDLE on a socket to number of seconds the connection is idle before keepalive probes are sent
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen) < 0)
    {
        dlog("Failed to set TCP_KEEPIDLE option on socket, %s\n", strerror(errno));
        return TCP_ERR_RC;
    }

    optval = cnt; // set TCP_KEEPCNT on a socket to number of keepalive probes before dropping connection
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen) < 0)
    {
        dlog("Failed to set TCP_KEEPCNT option on socket, %s\n", strerror(errno));
        return TCP_ERR_RC;
    }

    optval = intvl; // set TCP_KEEPINTVL on a socket to number of seconds between keepalive probes
    if(setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, optlen) < 0)
    {
        dlog("Failed to set TCP_KEEPINTVL option on socket, %s\n", strerror(errno));
        return TCP_ERR_RC;
    }
    
    return 0;
}

/*----------------------------------------------------------------------------
 * sockreuse
 *----------------------------------------------------------------------------*/
int SockLib::sockreuse(int socket_fd)
{
    int         optval;
    socklen_t   optlen = sizeof(optval);

    optval = 1; // set SO_REUSEADDR on a socket to true (1)
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
    {
        dlog("Failed to set SO_REUSEADDR option on socket, %s\n", strerror(errno));
        return SOCK_ERR_RC;
    }
    
    return 0;
}

/*----------------------------------------------------------------------------
 * socknonblock
 *----------------------------------------------------------------------------*/
int SockLib::socknonblock(int socket_fd)
{
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if(flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        dlog("Failed to make socket non-blocking, %s\n", strerror(errno));
        return SOCK_ERR_RC;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * sockmulticast
 *----------------------------------------------------------------------------*/
int SockLib::sockmulticast(int socket_fd, const char* group)
{
    struct ip_mreq  optval;
    socklen_t       optlen = sizeof(optval);

    optval.imr_multiaddr.s_addr = inet_addr(group);
    optval.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&optval, optlen) < 0)
    {
        dlog("Failed to set IP_ADD_MEMBERSHIP option on socket, %s\n", strerror(errno));
        return SOCK_ERR_RC;
    }
    
    return 0;
}
