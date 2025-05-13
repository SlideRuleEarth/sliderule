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

#include "OsApi.h"
#include "SockLib.h"

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
#include <exception>

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define IPV4_MULTICAST_START    0xE0000000
#define IPV4_MULTICAST_STOP     0xF0000000
#define ERRMSG_BUF_SIZE         256

/******************************************************************************
 * TYPEDEFS
 ******************************************************************************/

typedef struct sockaddr_in  socket_address_t;
typedef struct sockaddr     client_address_t;

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* SockLib::IPV4_ENV_VAR_NAME = "IPV4";

bool SockLib::signal_exit = false;
char SockLib::local_host_name[HOST_STR_LEN];
char SockLib::ipv4[IPV4_STR_LEN];

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * init
 *----------------------------------------------------------------------------*/
void SockLib::init()
{
    /* Initialize to Defaults */
    snprintf(local_host_name, HOST_STR_LEN, "%s", "unknown_host");
    snprintf(ipv4, IPV4_STR_LEN, "%s", "127.0.0.1");

    /* Attempt to Get Host Information */
    struct hostent* host = NULL;
    if(gethostname(local_host_name, HOST_STR_LEN) != -1)
    {
        host = gethostbyname(local_host_name); // NOLINT(concurrency-mt-unsafe)
    }

    /* Attempt to Get IP Address from Environment */
    const char* ip_from_env = getenv(IPV4_ENV_VAR_NAME); // NOLINT(concurrency-mt-unsafe)

    /* Get IP Address */
    if(ip_from_env)
    {
        snprintf(ipv4, IPV4_STR_LEN, "%s", ip_from_env);
    }
    else if(host != NULL)
    {
        const uint32_t addr = reinterpret_cast<struct in_addr*>(host->h_addr_list[0])->s_addr;
        snprintf(ipv4, IPV4_STR_LEN, "%u.%u.%u.%u", addr & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >> 24) & 0xFF);
    }
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
int SockLib::sockstream(const char* ip_addr, int port, bool is_server, const bool* block)
{
    /* Create initial socket */
    const int sock = sockcreate(SOCK_STREAM, ip_addr, port, is_server, block);
    if(sock == INVALID_RC) return INVALID_RC;

    if(!is_server) // client
    {
        return sock;
    }

    // server
    const int listen_socket = sock;
    int server_socket = INVALID_RC;
    char err_buf[ERRMSG_BUF_SIZE];

    /* Make Socket a Listen Socket */
    if(listen(listen_socket, 1) != 0)
    {
        dlog("Failed to mark socket bound to %s:%d as a listen socket, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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

    /* Close Listen Socket */
    sockclose(listen_socket);

    /* Check Acceptance */
    if(server_socket < 0)
    {
        dlog("Failed to accept connection on %s:%d, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return INVALID_RC;
    }

    /* Set Keep Alive on Socket */
    if(sockkeepalive(server_socket) < 0)
    {
        dlog("Failed to set keep alive on %s:%d, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        sockclose(server_socket);
        return INVALID_RC;
    }

    /* Make Socket Non-Blocking */
    if(socknonblock(server_socket) < 0)
    {
        dlog("Failed to set non-blocking on %s:%d, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        sockclose(server_socket);
        return INVALID_RC;
    }

    /* Return Server Socket */
    return server_socket;
}

/*----------------------------------------------------------------------------
 * sockdatagram
 *----------------------------------------------------------------------------*/
int SockLib::sockdatagram(const char* ip_addr, int port, bool is_server, const bool* block, const char* multicast_group)
{
    /* Create initial socket */
    const int sock = sockcreate(SOCK_DGRAM, ip_addr, port, is_server, block);
    if(sock == INVALID_RC) return INVALID_RC;

    /* Set Options for Multicast  - IPv4 only */
    if(multicast_group)
    {
        struct in_addr ipv4_addr;
        if(inet_pton(AF_INET, multicast_group, &ipv4_addr) == 1)
        {
            const uint32_t naddr = htonl(ipv4_addr.s_addr);
            if(naddr >= IPV4_MULTICAST_START && naddr < IPV4_MULTICAST_STOP)
            {
                if(sockmulticast(sock, multicast_group) < 0)
                {
                    sockclose(sock);
                    return INVALID_RC;
                }
                dlog("Configured socket on %s:%d to receive multicast packets on %s", ip_addr, port, multicast_group);
            }
            else
            {
                dlog("Invalid multicast group address %s - %08X", multicast_group, naddr  );
            }
        }
        else
        {
            dlog("Currently only IPv4 group addresses supported: %s", multicast_group);
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
    int revents = POLLOUT;
    int c = TIMEOUT_RC;

    /* Check Sock */
    if(fd == INVALID_RC)
    {
        if(timeout != IO_CHECK) OsApi::performIOTimeout();
        return TIMEOUT_RC;
    }

    if(timeout != IO_CHECK)
    {
        int activity = 1;

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
            char err_buf[ERRMSG_BUF_SIZE];
            dlog("Failed (%d) to send data to ready socket [0x%0X]: %s", c, revents, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
            char err_buf[ERRMSG_BUF_SIZE];
            dlog("Failed (%d) to receive data from ready socket [0x%0X]: %s", c, revents, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
    if(getsockname(fd, reinterpret_cast<struct sockaddr *>(&local_addr), &addr_size) < 0) return -1;
    if(getpeername(fd, reinterpret_cast<struct sockaddr *>(&remote_addr), &addr_size) < 0) return -1;

    /* Populate Local IP Address */
    if(local_ipaddr)
    {
        *local_ipaddr = new char[17];
        strncpy(*local_ipaddr, inet_ntoa(local_addr.sin_addr), 16); // NOLINT(concurrency-mt-unsafe)
        (*local_ipaddr)[15] = '\0';
    }

    /* Populate Local Port */
    if(local_port)
    {
        *local_port = ntohs(local_addr.sin_port);
    }

    /* Populate Remote IP Address */
    if(remote_ipaddr)
    {
        *remote_ipaddr = new char[17];
        strncpy(*remote_ipaddr, inet_ntoa(remote_addr.sin_addr), 16); // NOLINT(concurrency-mt-unsafe)
        (*remote_ipaddr)[15] = '\0';
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
    if( fd != INVALID_RC)
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    }
}

/*----------------------------------------------------------------------------
 * startserver
 *----------------------------------------------------------------------------*/
int SockLib::startserver(const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, const bool* active, void* parm, bool* listening)
{
    int status = 0;

    /* Initialize Connection Variables */
    int num_sockets = 0;
    const int max_num_sockets = max_num_connections + 1; // add in listener
    struct pollfd* polllist = new struct pollfd [max_num_sockets];

    /* Create Listen Socket */
    int listen_socket = sockcreate(SOCK_STREAM, ip_addr, port, true, NULL);
    if(listen_socket >= 0)
    {
        /* Make Socket a Listen Socket */
        if(listen(listen_socket, 1) == 0)
        {
            /* Set Listener in Poll List */
            //dlog("Established listener socket on %s:%d", ip_addr ? ip_addr : "0.0.0.0", port);
            num_sockets++; // for listener socket
            polllist[0].fd = listen_socket;
            polllist[0].events = POLLIN; // start out listening for new connections
            polllist[0].revents = 0;
            if(listening) *listening = true;
        }
        else
        {
            char err_buf[ERRMSG_BUF_SIZE];
            dlog("Failed to mark socket bound to %s:%d as a listen socket, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
            listen_socket = INVALID_RC;
            status = -1;
        }
    }
    else
    {
        dlog("Unable to establish socket server on %s:%d, failed to create listen socket", ip_addr ? ip_addr : "0.0.0.0", port);
        listen_socket = INVALID_RC;
        status = -1;
    }

    if(listen_socket != INVALID_RC)
    {
        try
        {
            /* Loop While Active */
            while(*active)
            {
                /* Build Polling Flags */
                for(int j = 1; j < num_sockets; j++)
                {
                    on_poll(polllist[j].fd, &polllist[j].events, parm);
                }

                /* Poll On All Connections */
                int activity = 0;
                do activity = poll(polllist, num_sockets, 100); // 10Hz
                while(activity == -1 && (errno == EINTR || errno == EAGAIN));

                /* Check Activity */
                if(activity < 0)
                {
                    char errmsg[ERRMSG_BUF_SIZE];
                    snprintf(errmsg, ERRMSG_BUF_SIZE, "Poll error (%d)... exiting server", errno);
                    throw std::runtime_error(errmsg);
                }

                /* Handle Existing Connections  */
                int i = 1; // skip listener socket
                while(i < num_sockets)
                {
                    bool valid_fd = true;
                    int cb_stat = 0;
                    char err_buf[ERRMSG_BUF_SIZE];

                    /* Handle Errors */
                    if(polllist[i].revents & POLLERR)
                    {
                        int error = 0;
                        socklen_t errlen = sizeof(error);
                        getsockopt(polllist[i].fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<void*>(&error), &errlen);
#ifdef __DEBUG__
                        /* With server keeping sockets alive for HTTP/1.1 requests, the client will close the connection.
                         * It may do it gracefully or abruptly.  If it does it abruptly (curlib3 socket pool timeout)
                         * the server will see a POLLERR and ECONNRESET error, this is normal and should not be logged as an error. */
                        dlog("Poll error (%d) detected [0x%X] on server socket <%d>: %s", error, polllist[i].revents, polllist[i].fd, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
#endif
                        cb_stat = -1; // treat like a callback error or disconnect
                    }
                    else if(polllist[i].revents & POLLNVAL)
                    {
                        dlog("Socket <%d> not open, yet trying to poll: %s", polllist[i].fd, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
                        if(valid_fd) sockclose(polllist[i].fd);
                        valid_fd = false;
                    }

                    /* Check if FD still Valid */
                    if(!valid_fd)
                    {
                        /* Remove from Polling */
                        const int connections_left = num_sockets - 1;
                        if(i < connections_left)
                        {
                            memmove(&polllist[i], &polllist[i+1], (connections_left - i) * sizeof(struct pollfd));
                        }

                        /* Decrement Number of Connections */
                        num_sockets = connections_left;
                        if(num_sockets < max_num_sockets)
                        {
                            /* start listener socket polling for new connections */
                            polllist[0].events |= POLLIN;
                        }
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
                    int error = 0;
                    socklen_t errlen = sizeof(error);
                    getsockopt(polllist[i].fd, SOL_SOCKET, SO_ERROR, static_cast<void*>(&error), &errlen);

                    char err_buf[ERRMSG_BUF_SIZE];
                    dlog("Poll error (%d) detected [0x%X] on listener socket: %s", error, polllist[0].revents, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
                }
                else if(polllist[0].revents & POLLIN)
                {
                    if(num_sockets < max_num_sockets)
                    {
                        /* Accept Next Connection */
                        client_address_t    client_address;
                        socklen_t           address_length = sizeof(socket_address_t);

                        const int client_socket = accept(listen_socket, &client_address, &address_length);
                        if(client_socket != -1)
                        {
                            /* Set Non-Blocking */
                            if(socknonblock(client_socket) == 0)
                            {
                                /* Call On Activity Call-Back */
                                if(on_act(client_socket, IO_CONNECT_FLAG, parm) >= 0)
                                {
                                    /* Populate New Connection */
                                    polllist[num_sockets].fd = client_socket;
                                    polllist[num_sockets].events = POLLHUP; // always listen for haning up
                                    polllist[num_sockets].revents = 0;

                                    /* Increment Number of Connections */
                                    num_sockets++;
                                    if(num_sockets >= max_num_sockets)
                                    {
                                        /* stop listener socket polling for new connections */
                                        polllist[0].events &= ~POLLIN;
                                    }
                                }
                                else
                                {
                                    SockLib::sockclose(client_socket);
                                }
                            }
                            else
                            {
                                dlog("Failed to set socket to non-blocking %s:%d", ip_addr ? ip_addr : "0.0.0.0", port);
                            }
                        }
                        else
                        {
                            dlog("Failed to set accept connection on %s:%d", ip_addr ? ip_addr : "0.0.0.0", port);
                        }
                    }
                    else
                    {
                        dlog("Maximum number of sockets exceeded: %d", max_num_sockets);
                    }
                }
            }
        }
        catch(const std::exception& e)
        {
            dlog("Caught fatal exception, aborting http server thread: %s", e.what());
            status = -1;
        }

        /* Close Listening Socket */
        sockclose(listen_socket);
        if(listening) *listening = false;
    }

    /* Disconnect Existing Connections (*/
    for(int i = 1; i < num_sockets; i++)
    {
        try
        {
            on_act(polllist[i].fd, IO_DISCONNECT_FLAG, parm);
        }
        catch(const std::exception& e)
        {
            dlog("Caught exception on disconnect: %s", e.what());
            status = -1;
        }
        sockclose(polllist[i].fd);
    }

    /* Clean Up Allocated Memory */
    delete [] polllist;

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * startclient
 *----------------------------------------------------------------------------*/
int SockLib::startclient(const char* ip_addr, int port, int max_num_connections, onPollHandler_t on_poll, onActiveHandler_t on_act, const bool* active, void* parm, bool* connected)
{
    /* Initialize Connection Variables */
    bool _connected = false;
    const int max_num_sockets = max_num_connections + 1; // for listener socket
    int num_sockets = 0;
    struct pollfd polllist[1];

    /* Loop While Active */
    while(*active)
    {
        /* Build Poll Events */
        on_poll(0, &polllist[0].events, parm);

        /* If Not Connected */
        if(!_connected)
        {
            /* Check for Connection Upper Limit */
            if(max_num_sockets > 0 && num_sockets >= max_num_sockets)
            {
                dlog("Maximum number of connections reached: %d", max_num_connections);
                return -1;
            }

            /* Connect Client Socket */
            const int client_socket = sockstream(ip_addr, port, false, NULL);
            if(client_socket >= 0)
            {
                /* Call Connection Callback */
                if(on_act(client_socket, IO_CONNECT_FLAG, parm) < 0)
                {
                    dlog("Callback on connection returned error, exiting");
                    sockclose(client_socket);
                    return -1;
                }

                /* Update Connection Variables */
                dlog("Client socket <%d> connection made to %s:%d", client_socket, ip_addr ? ip_addr : "0.0.0.0", port);
                polllist[0].fd = client_socket;
                _connected = true;
                if(connected) *connected = true;
                num_sockets++;
            }
            else // error connected
            {
                if(client_socket != WOULDBLOCK_RC)
                {
                    dlog("Unable to create client socket on %s:%d", ip_addr ? ip_addr : "0.0.0.0", port);
                    return -1;
                }

                /* Timeout */
                OsApi::performIOTimeout();
            }
        }

        /* If Connected */
        if(_connected)
        {
            /* Initialize Polling Structure */
            polllist[0].revents = 0;

            /* Perform Poll */
            int activity = 0;
            do activity = poll(polllist, 1, SYS_TIMEOUT);
            while(activity == -1 && (errno == EINTR || errno == EAGAIN));

            /* Handle Errors */
            bool valid_fd = true;
            int cb_stat = 0;
            char err_buf[ERRMSG_BUF_SIZE];
            if(polllist[0].revents & POLLERR)
            {
                dlog("Poll error detected on client socket <%d>: %s", polllist[0].fd, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
                cb_stat = -1; // treat like a callback error or disconnect
            }
            else if(polllist[0].revents & POLLNVAL)
            {
                dlog("Socket <%d> not open, yet trying to poll: %s", polllist[0].fd, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
                dlog("Disconnect client socket <%d> from client socket %s:%d", polllist[0].fd, ip_addr ? ip_addr : "0.0.0.0", port);
                on_act(polllist[0].fd, IO_DISCONNECT_FLAG, parm); // no sense checking error, nothing to do
                SockLib::sockclose(polllist[0].fd);
                valid_fd = false;
            }

            /* Check if FD still Valid */
            if(!valid_fd)
            {
                _connected = false;
                if(connected) *connected = false;
            }
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * sockhost
 *----------------------------------------------------------------------------*/
const char* SockLib::sockhost (void)
{
    return local_host_name;
}

/*----------------------------------------------------------------------------
 * sockipv4
 *----------------------------------------------------------------------------*/
const char* SockLib::sockipv4 (void)
{
    return ipv4;
}

/*----------------------------------------------------------------------------
 * sockcreate
 *----------------------------------------------------------------------------*/
int SockLib::sockcreate(int type, const char* ip_addr, int port, bool is_server, const bool* block)
{
    struct addrinfo     hints;
    struct addrinfo*    result;
    struct addrinfo*    rp;
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
        dlog("Failed to get address info for %s:%d, %s", ip_addr, port, gai_strerror(status));
        return TCP_ERR_RC;
    }

    char err_buf[ERRMSG_BUF_SIZE];

    /* Try each address until we successfully connect. */
    for(rp = result; rp != NULL; rp = rp->ai_next)
    {
        /* Get address information */
        getnameinfo(rp->ai_addr,rp->ai_addrlen, host, HOST_STR_LEN, serv, SERV_STR_LEN, NI_NUMERICHOST | NI_NUMERICSERV);

        /* Create socket */
        sock = socket(rp->ai_family, rp->ai_socktype, 0);
        if(sock < 0)
        {
            dlog("Failed to open socket for %s:%s, %s", host, serv, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
            continue;
        }

        if(is_server)
        {
            /* Set Reuse Option on Listen Socket */
            if(sockreuse(sock) < 0)
            {
                dlog("Failed to set reuse on socket %s:%s, %s", host, serv, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
                close(sock);
                continue;
            }

            /* Bind Socket */
            status = bind(sock, rp->ai_addr, rp->ai_addrlen);
            if(status < 0)
            {
                dlog("Failed to bind socket to %s:%s, %s", host, serv, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
                    dlog("Failed to connect socket to %s:%s... %s", host, serv, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
                    OsApi::performIOTimeout();
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
    if(rp == NULL)
    {
        return WOULDBLOCK_RC;
    }

    /* Set Keep Alive on Socket */
    if(type == SOCK_STREAM)
    {
        if(sockkeepalive(sock) < 0)
        {
            dlog("Failed to set keep alive on %s:%d, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
            sockclose(sock);
            return TCP_ERR_RC;
        }

        /* Make Socket Non-Blocking*/
        if(socknonblock(sock) < 0)
        {
            dlog("Failed to set non-blocking on %s:%d, %s", ip_addr ? ip_addr : "0.0.0.0", port, strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
    int               optval;
    const socklen_t   optlen = sizeof(optval);
    char              err_buf[ERRMSG_BUF_SIZE];

    optval = 1; // set SO_KEEPALIVE on a socket to true (1)
    if (setsockopt(socket_fd, SOL_SOCKET, SO_KEEPALIVE, &optval, optlen) < 0)
    {
        dlog("Failed to set SO_KEEPALIVE option on socket, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return TCP_ERR_RC;
    }

    optval = idle; // set TCP_KEEPIDLE on a socket to number of seconds the connection is idle before keepalive probes are sent
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPIDLE, &optval, optlen) < 0)
    {
        dlog("Failed to set TCP_KEEPIDLE option on socket, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return TCP_ERR_RC;
    }

    optval = cnt; // set TCP_KEEPCNT on a socket to number of keepalive probes before dropping connection
    if (setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPCNT, &optval, optlen) < 0)
    {
        dlog("Failed to set TCP_KEEPCNT option on socket, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return TCP_ERR_RC;
    }

    optval = intvl; // set TCP_KEEPINTVL on a socket to number of seconds between keepalive probes
    if(setsockopt(socket_fd, IPPROTO_TCP, TCP_KEEPINTVL, &optval, optlen) < 0)
    {
        dlog("Failed to set TCP_KEEPINTVL option on socket, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return TCP_ERR_RC;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * sockreuse
 *----------------------------------------------------------------------------*/
int SockLib::sockreuse(int socket_fd)
{
    int               optval;
    const socklen_t   optlen = sizeof(optval);

    optval = 1; // set SO_REUSEADDR on a socket to true (1)
    if(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) < 0)
    {
        char err_buf[ERRMSG_BUF_SIZE];
        dlog("Failed to set SO_REUSEADDR option on socket, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return SOCK_ERR_RC;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * socknonblock
 *----------------------------------------------------------------------------*/
int SockLib::socknonblock(int socket_fd)
{
    const int flags = fcntl(socket_fd, F_GETFL, 0);
    if(flags < 0 || fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) < 0)
    {
        char err_buf[ERRMSG_BUF_SIZE];
        dlog("Failed to make socket non-blocking, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
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
    const socklen_t optlen = sizeof(optval);

    optval.imr_multiaddr.s_addr = inet_addr(group);
    optval.imr_interface.s_addr = htonl(INADDR_ANY);
    if(setsockopt(socket_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char*>(&optval), optlen) < 0)
    {
        char err_buf[ERRMSG_BUF_SIZE];
        dlog("Failed to set IP_ADD_MEMBERSHIP option on socket, %s", strerror_r(errno, err_buf, sizeof(err_buf))); // Get thread-safe error message
        return SOCK_ERR_RC;
    }

    return 0;
}
