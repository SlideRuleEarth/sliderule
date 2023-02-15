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

#include "ClusterSocket.h"
#include "Ordering.h"
#include "EventLib.h"
#include "OsApi.h"
#include "StringLib.h"
#include "TimeLib.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - cluster(<role>, <protocol>, <ip_addr>, <port>, <"SERVER"|"CLIENT">, <stream name>)
 *
 *  where <protocol> is:
 *
 *      QUEUE implements a distribute and collect network architecture.
 *      Once a connection is made, once per second, the percentage full of the outgoing stream
 *      is calculated and sent as an 8-bit number to the WRITER.  The WRITER will only
 *      send more data if the percentage is below a threshold.
 *
 *      BUS implements a publish and subscribe network architecture.
 *      Once a connection is made, data will be read as fast as possible and posted to the stream.
 *----------------------------------------------------------------------------*/
int ClusterSocket::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        int         role      = (int)getLuaInteger(L, 1);
        int         protocol  = (int)getLuaInteger(L, 2);
        const char* ip_addr   = getLuaString(L, 3);
        int         port      = (int)getLuaInteger(L, 4);
        bool        is_server = getLuaBoolean(L, 5);
        const char* q_name    = getLuaString(L, 6);

        /* Get Server Parameter */
        if(is_server)
        {
            if(StringLib::match(ip_addr, "0.0.0.0") || StringLib::match(ip_addr, "*"))
            {
                ip_addr = NULL;
            }
        }

        /* Return File Device Object */
        return createLuaObject(L, new ClusterSocket(L, ip_addr, port, (ClusterSocket::role_t)role, (ClusterSocket::protocol_t)protocol, is_server, false, q_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating ClusterSocket: %s", e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
ClusterSocket::ClusterSocket(lua_State* L, const char* _ip_addr, int _port, role_t _role, protocol_t _protocol, bool _is_server, bool _is_blind, const char* passthruq):
    TcpSocket(L, INVALID_RC, _ip_addr, _port, _role),
    read_connections(MAX_NUM_CONNECTIONS),
    write_connections(MAX_NUM_CONNECTIONS)
{
    protocol = _protocol;
    is_server = _is_server;
    is_blind = _is_blind;

    if(passthruq)
    {
        sockqname = StringLib::duplicate(passthruq);
    }
    else
    {
        char qname_buf[64];
        sockqname = StringLib::duplicate(StringLib::format(qname_buf, 64, "sockq_%s:%d", _ip_addr, _port));
    }

    pubsockq = new Publisher(sockqname);
    if( (role == WRITER && protocol == BUS) ||  // bus writers create their own subscription of opportunity per connection
        (role == READER && passthruq) )         // passthru readers assume an external subscription to the queue
    {
        subsockq = NULL;
    }
    else
    {
        subsockq = new Subscriber(sockqname);
    }

    spin_block = false;
    connecting = true;
    connector = new Thread(connectionThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
ClusterSocket::~ClusterSocket(void)
{
    connecting = false;
    delete connector;

    delete [] sockqname;
    delete pubsockq;
    if(subsockq) delete subsockq;

    /*
     * If connector thread exits before lost connection is detected
     * onDisconnect method will not be called and connection
     * memory will not be freed.
     * This cleanup code must be called after the connector thread
     * has been distroyed to avoid race condition.
     */
    read_connection_t*  readCon;
    write_connection_t* writeCon;

    int fd = read_connections.first( &readCon );
    while (fd != (int)INVALID_KEY)
    {
        SockLib::sockclose(fd);
        if(readCon->payload) delete [] readCon->payload;
        if(readCon) delete readCon;
        fd = read_connections.next( &readCon );
    }

    fd = write_connections.first( &writeCon );
    while (fd != (int)INVALID_KEY)
    {
        SockLib::sockclose(fd);
        if(writeCon) delete writeCon;
        fd = write_connections.next( &writeCon );
    }
}

/*----------------------------------------------------------------------------
 * isConnected
 *----------------------------------------------------------------------------*/
bool ClusterSocket::isConnected(int num_connections)
{
    return (read_connections.length() + write_connections.length() >= num_connections);
}

/*----------------------------------------------------------------------------
 * closeConnection
 *
 *  Notes: does not close the listener... just anything connected by it
 *----------------------------------------------------------------------------*/
void ClusterSocket::closeConnection(void)
{
    connecting = false;
}

/*----------------------------------------------------------------------------
 * writeBuffer
 *
 *  Notes: read meter and send data if below threshold
 *----------------------------------------------------------------------------*/
int ClusterSocket::writeBuffer(const void *buf, int len, int timeout)
{
    assert(role == WRITER);
    if(buf == NULL || len <= 0)
    {
        return TIMEOUT_RC;
    }
    else if(len <= MAX_MSG_SIZE)
    {
        int status = pubsockq->postCopy(buf, len, timeout);
        if(status > 0)                                  return status;
        else if(status == MsgQ::STATE_NO_SUBSCRIBERS)   return len;
        else if(status == MsgQ::STATE_TIMEOUT)          return TIMEOUT_RC;
        else                                            return SOCK_ERR_RC;
    }
    else
    {
        return PARM_ERR_RC;
    }
}

/*----------------------------------------------------------------------------
 * readBuffer
 *
 *  Notes: send meter and read data
 *----------------------------------------------------------------------------*/
int ClusterSocket::readBuffer(void *buf, int len, int timeout)
{
    assert(role == READER);
    if(buf && subsockq)
    {
        int bytes = subsockq->receiveCopy(buf, len, timeout);
        if(bytes > 0)                           return bytes;
        else if(bytes == MsgQ::STATE_TIMEOUT)   return TIMEOUT_RC;
        else                                    return SOCK_ERR_RC;
    }
    else
    {
        return PARM_ERR_RC;
    }
}

/*----------------------------------------------------------------------------
 * connectionThread
 *----------------------------------------------------------------------------*/
void* ClusterSocket::connectionThread(void* parm)
{
    ClusterSocket* s = (ClusterSocket*)parm;

    int status = 0;

    if(s->is_server) SockLib::startserver (s->getIpAddr(), s->getPort(), MAX_NUM_CONNECTIONS, pollHandler, activeHandler, &s->connecting, (void*)s);
    else             SockLib::startclient (s->getIpAddr(), s->getPort(), MAX_NUM_CONNECTIONS, pollHandler, activeHandler, &s->connecting, (void*)s);

    if(status < 0)   mlog(CRITICAL, "Failed to establish cluster %s socket on %s:%d (%d)", s->is_server ? "server" : "client", s->getIpAddr(), s->getPort(), status);

    return NULL;
}

/*----------------------------------------------------------------------------
 * pollHandler
 *
 *  Notes: provides the flags back to the poll function
 *----------------------------------------------------------------------------*/
int ClusterSocket::pollHandler(int fd, short* events, void* parm)
{
    (void)fd;

    ClusterSocket* s = (ClusterSocket*)parm;

    /* Set Polling Flags */
    *events = IO_READ_FLAG;
    if(s->role == WRITER)
    {
        *events |= IO_WRITE_FLAG;
        if(s->pubsockq->getCount() > 0)
        {
            s->spin_block = false;
        }
    }

    /* Check Spinning */
    if(s->spin_block)
    {
        mlog(WARNING, "Executing spin block for cluster socket<%d> %s:%d", s->role, s->getIpAddr(), s->getPort());
        OsApi::sleep(1);
    }
    else
    {
        s->spin_block = true;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * activeHandler
 *
 *  Notes: performed on activity returned from poll
 *----------------------------------------------------------------------------*/
int ClusterSocket::activeHandler(int fd, int flags, void* parm)
{
    ClusterSocket* s = (ClusterSocket*)parm;

    int rc = 0;

    if((flags & IO_READ_FLAG)       && (s->onRead(fd)       < 0))   rc = -1;
    if((flags & IO_WRITE_FLAG)      && (s->onWrite(fd)      < 0))   rc = -1;
    if((flags & IO_ALIVE_FLAG)      && (s->onAlive(fd)      < 0))   rc = -1;
    if((flags & IO_CONNECT_FLAG)    && (s->onConnect(fd)    < 0))   rc = -1;
    if((flags & IO_DISCONNECT_FLAG) && (s->onDisconnect(fd) < 0))   rc = -1;

    return rc;
}

/*----------------------------------------------------------------------------
 * onRead
 *
 *  Notes: performed for every connection that is ready to have data read from it
 *----------------------------------------------------------------------------*/
int ClusterSocket::onRead(int fd)
{
    try
    {
        if(role == READER)
        {
            read_connection_t* connection = read_connections[fd];

            /* Check for Buffer Complete */
            if(connection->buffer_index >= connection->buffer_size)
            {
                connection->buffer_index = 0;
                connection->buffer_size = 0;
            }

            /* Read More Data */
            int bytes_left = MSG_BUFFER_SIZE - connection->buffer_size;
            if(bytes_left > MIN_BUFFER_SIZE)
            {
                int bytes = SockLib::sockrecv(fd, &connection->buffer[connection->buffer_size], bytes_left, IO_CHECK);
                if(bytes > 0)
                {
                    connection->buffer_size += bytes;
                    spin_block = false;
                }
            }
        }
        else if(role == WRITER)
        {
            write_connection_t* connection = write_connections[fd];

            /* Read Meter */
            int meters_read = 0;
            uint8_t meter_buf[METER_BUF_SIZE];
            while((meters_read = SockLib::sockrecv(fd, meter_buf, METER_BUF_SIZE, IO_CHECK)) > 0)
            {
                connection->meter = meter_buf[meters_read - 1];
                spin_block = false;
            }
        }
    }
    catch(RunTimeException& e)
    {
        mlog(e.level(), "Cluster socket on %s:%d failed to retrieve connection information for file descriptor %d: %s", getIpAddr(), getPort(), fd, e.what());
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * onWrite
 *
 *  Notes: performed for every connection that is ready to have data written to it
 *----------------------------------------------------------------------------*/
int ClusterSocket::onWrite(int fd)
{
    assert(role == WRITER);

    try
    {
        write_connection_t* connection = write_connections[fd];

        /* Check Meter */
        if(connection->meter < METER_SEND_THRESH || is_blind)
        {
            /* While room in buffer for at least header */
            while(connection->buffer_index < (MSG_BUFFER_SIZE - MSG_HDR_SIZE))
            {
                /* Check If Payload Reference Has More Data */
                if(connection->payload_left > 0)
                {
                    /* Populate Buffer */
                    uint32_t bytes_left = MSG_BUFFER_SIZE - connection->buffer_index;
                    uint32_t cpylen = connection->payload_left < bytes_left ? connection->payload_left : bytes_left;
                    int payload_index = connection->payload_ref.size - connection->payload_left;
                    uint8_t* payload_buffer = (uint8_t*)connection->payload_ref.data;
                    memcpy(&connection->buffer[connection->buffer_index], &payload_buffer[payload_index], cpylen);
                    connection->buffer_index += cpylen;
                    connection->payload_left -= cpylen;
                }

                /* Get New Payload Reference */
                if((connection->payload_left == 0) &&
                   (connection->buffer_index < (MSG_BUFFER_SIZE - MSG_HDR_SIZE)))
                {
                    int status = connection->subconnq->receiveRef(connection->payload_ref, IO_CHECK);
                    if(status > 0)
                    {
                        /* Populate Header */
                        connection->buffer[connection->buffer_index++] = (uint8_t)(connection->payload_ref.size >> 24);
                        connection->buffer[connection->buffer_index++] = (uint8_t)(connection->payload_ref.size >> 16);
                        connection->buffer[connection->buffer_index++] = (uint8_t)(connection->payload_ref.size >>  8);
                        connection->buffer[connection->buffer_index++] = (uint8_t)(connection->payload_ref.size >>  0);

                        /* Populate Rest of Buffer */
                        int bytes_left = MSG_BUFFER_SIZE - connection->buffer_index;
                        int cpylen = connection->payload_ref.size < bytes_left ? connection->payload_ref.size : bytes_left;
                        memcpy(&connection->buffer[connection->buffer_index], connection->payload_ref.data, cpylen);
                        connection->buffer_index += cpylen;

                        /* Calculate Payload Left and Dereference if Payload Fully Buffered */
                        connection->payload_left = connection->payload_ref.size - cpylen;
                        if(connection->payload_left == 0)
                        {
                            connection->subconnq->dereference(connection->payload_ref);
                        }

                        /* Release Spin Block */
                        spin_block = false;
                    }
                    else
                    {
                        // If queue is empty, then stop populating buffer
                        // and continue to code that sends it
                        break;
                    }
                }
            }

            /* Send Data */
            while(connection->bytes_processed < connection->buffer_index)
            {
                int bytes_left = connection->buffer_index - connection->bytes_processed;
                int bytes = SockLib::socksend(fd, &connection->buffer[connection->bytes_processed], bytes_left, IO_CHECK);
                if(bytes > 0)
                {
                    connection->bytes_processed += bytes;
                    spin_block = false;
                }
                else
                {
                    // Failed to send data on socket that was marked for writing;
                    // therefore return failure which will close socket
                    return -1;
                }
            }

            /* Check If Buffer Data Finished Being Sent */
            if(connection->bytes_processed == connection->buffer_index)
            {
                /* Reset Buffer Index */
                connection->buffer_index = 0;
                connection->bytes_processed = 0;

                /* Optimization - Send Unbuffered Payload Data */
                while(connection->payload_left > 0)
                {
                    int payload_index = connection->payload_ref.size - connection->payload_left;
                    uint8_t* byte_buffer = (uint8_t*)connection->payload_ref.data;
                    int bytes = SockLib::socksend(fd, &byte_buffer[payload_index], connection->payload_left, IO_CHECK);
                    if(bytes > 0)
                    {
                        connection->payload_left -= bytes;
                        spin_block = false;

                        /* Dereference If Payload Fully Sent */
                        if(connection->payload_left <= 0)
                        {
                            connection->subconnq->dereference(connection->payload_ref);
                        }
                    }
                    else
                    {
                        // don't return error since this is an optimization check
                        // instead stop for now, code will do the right thing on next
                        // poll cycle
                        break;
                    }
                }
            }
        }
    }
    catch(RunTimeException& e)
    {
        mlog(e.level(), "Cluster socket on %s:%d failed to retrieve connection information for file descriptor %d: %s", getIpAddr(), getPort(), fd, e.what());
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * onAlive
 *
 *  Notes: Performed for every existing connection
 *----------------------------------------------------------------------------*/
int ClusterSocket::onAlive(int fd)
{
    try
    {
        if(role == READER)
        {
            read_connection_t* connection = read_connections[fd];

            /* Send Meter */
            int64_t now = TimeLib::gpstime();
            if((now - connection->prev) > METER_PERIOD_MS)
            {
                uint8_t meter = 0;
                connection->prev = now;
                meter = qMeter();
                SockLib::socksend(fd, &meter, 1, IO_CHECK);
            }

            while(connection->buffer_index < connection->buffer_size)
            {
                /* Process Header */
                if(connection->payload_index < 0)
                {
                    int shift = 24 - ((connection->payload_index++ + MSG_HDR_SIZE) * 8);
                    uint32_t value = connection->buffer[connection->buffer_index++];
                    connection->payload_size |= value << shift;

                    /* Header Complete */
                    if(connection->payload_index == 0)
                    {
                        if(connection->payload_size > 0 && connection->payload_size <= MAX_MSG_SIZE)
                        {
                            connection->payload = new uint8_t [connection->payload_size];
                        }
                        else
                        {
                            mlog(CRITICAL, "Cluster socket on %s:%d attempted to read message of invalid size %u", getIpAddr(), getPort(), connection->payload_size);
                            connection->payload_size = 0;
                            return -1; // error... will cause connection to disconnect
                        }
                    }
                }
                /* Process Payload */
                else if (connection->payload_index <= connection->payload_size)
                {
                    int bytes_left = connection->buffer_size - connection->buffer_index;
                    int pay_bytes_left = connection->payload_size - connection->payload_index;
                    int cpylen = MIN(pay_bytes_left, bytes_left);
                    memcpy(&connection->payload[connection->payload_index], &connection->buffer[connection->buffer_index], cpylen);
                    connection->buffer_index += cpylen;
                    connection->payload_index += cpylen;

                    /* Payload Complete */
                    if(connection->payload_index >= connection->payload_size)
                    {
                        /* Publisher queue is single exit point for a cluster socket... block is appropriate below */
                        int status = pubsockq->postCopy(connection->payload, connection->payload_size, SYS_TIMEOUT);
                        if(status > 0 || is_blind)
                        {
                            delete [] connection->payload;
                            connection->payload = NULL;
                            connection->payload_size = 0;
                            connection->payload_index = -MSG_HDR_SIZE;
                            spin_block = false;
                        }
                        else
                        {
                            // If metering is working correctly, then this message should never come out.
                            // A timed out post indicates a full queue.  If the reader is able to send the
                            // meter to the writer, it then would indicate not to send anymore data.
                            mlog(CRITICAL, "Cluster socket timed out on post to %s", pubsockq->getName());
                            break; // exit loop to allow other processing to continue
                        }
                    }
                }
            }
        }
    }
    catch(RunTimeException& e)
    {
        mlog(e.level(), "Cluster socket on %s:%d failed to retrieve connection information for file descriptor %d: %s", getIpAddr(), getPort(), fd, e.what());
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * onConnect
 *
 *  Notes: performed on new connections when the connection is made
 *----------------------------------------------------------------------------*/
int ClusterSocket::onConnect(int fd)
{
    int status = 0;

    if(role == READER)
    {
        /* Populate Read Connection Structure */
        read_connection_t* connection = new read_connection_t;
        memset(connection, 0, sizeof(read_connection_t));
        connection->payload_index = -MSG_HDR_SIZE;

        /* Add to Read Connections */
        if(!read_connections.add(fd, connection, false))
        {
            mlog(CRITICAL, "Cluster socket failed to register file descriptor for read connection due to duplicate entry");
            status = -1;
        }
    }
    else if(role == WRITER)
    {
        write_connection_t* connection = new write_connection_t;
        memset(connection, 0, sizeof(write_connection_t));
        connection->meter = METER_SEND_THRESH;

        /* Initialize Subscriber for Connection */
        if(protocol == BUS)
        {
            connection->subconnq = new Subscriber(sockqname, MsgQ::SUBSCRIBER_OF_CONFIDENCE, MsgQ::CFG_DEPTH_STANDARD, MsgQ::CFG_SIZE_INFINITY);
        }
        else
        {
           connection->subconnq = subsockq;
        }

        /* Add to Write Connections */
        if(!write_connections.add(fd, connection, false))
        {
            mlog(CRITICAL, "Cluster socket failed to register file descriptor for read connection due to duplicate entry");
            if(role == WRITER && protocol == BUS && connection->subconnq) delete connection->subconnq;
            status = -1;
        }
    }
    else
    {
        assert(false); // error in code
    }

    return status;
}

/*----------------------------------------------------------------------------
 * onDisconnect
 *
 *  Notes: performed on disconnected connections
 *----------------------------------------------------------------------------*/
int ClusterSocket::onDisconnect(int fd)
{
    int status = 0;

    try
    {
        if(role == READER)
        {
            read_connection_t* connection = read_connections[fd];
            if(connection->payload) delete [] connection->payload;
            delete connection;
            if(!read_connections.remove(fd))
            {
                mlog(CRITICAL, "Cluster socket on %s:%d failed to remove connection information for reader file descriptor %d", getIpAddr(), getPort(), fd);
                status = -1;
            }
        }
        if(role == WRITER)
        {
            write_connection_t* connection = write_connections[fd];
            if(protocol == BUS && connection->subconnq) delete connection->subconnq;
            delete connection;
            if(!write_connections.remove(fd))
            {
                mlog(CRITICAL, "Cluster socket on %s:%d failed to remove connection information for writer file descriptor %d", getIpAddr(), getPort(), fd);
                status = -1;
            }
        }

    }
    catch(RunTimeException& e)
    {
        mlog(e.level(), "Cluster socket on %s:%d failed to retrieve connection information for file descriptor %d: %s", getIpAddr(), getPort(), fd, e.what());
        status = -1;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * qMeter
 *----------------------------------------------------------------------------*/
uint8_t ClusterSocket::qMeter(void)
{
    int depth = pubsockq->getDepth();
    if(depth)   return (uint8_t)((pubsockq->getCount() * 255) / depth);
    else        return 0;
}
