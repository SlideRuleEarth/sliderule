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

/*
 * INTERFACE DATASRV_INT tcpip_client_interface.rb localhost 33501 33502 nil nil LENGTH 32 16 0 1 LITTLE_ENDIAN 6 0x52544150 nil true
 */

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CosmosInterface.h"
#include "CommandProcessor.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CosmosInterface::TYPE = "CosmosInterface";
const char* CosmosInterface::SYNC_PATTERN = "RTAP";

/******************************************************************************
 * PUBLIC FUNCTIONS (STATIC)
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* CosmosInterface::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    /* Parse Inputs */
    const char*     tlmq_name       = StringLib::checkNullStr(argv[0]);
    const char*     cmdq_name       = StringLib::checkNullStr(argv[1]);
    const char*     tlm_ip          = StringLib::checkNullStr(argv[2]);
    const char*     tlm_port_str    = StringLib::checkNullStr(argv[3]);
    const char*     cmd_ip          = StringLib::checkNullStr(argv[4]);
    const char*     cmd_port_str    = StringLib::checkNullStr(argv[5]);

    long            tlm_port        = 0;
    long            cmd_port        = 0;

    /* Check Parameters */
    if(!tlmq_name || !cmdq_name || !tlm_ip || !cmd_ip)
    {
        mlog(CRITICAL, "No NULL values allowed when creating a %s\n", TYPE);
        return NULL;
    }
    else if(!StringLib::str2long(tlm_port_str, &tlm_port))
    {
        mlog(CRITICAL, "Invalid value provided for telemetry port: %s\n", tlm_port_str);
        return NULL;
    }
    else if(!StringLib::str2long(cmd_port_str, &cmd_port))
    {
        mlog(CRITICAL, "Invalid value provided for command port: %s\n", cmd_port_str);
        return NULL;
    }

    /* Check Port Numbers */
    if(tlm_port < 0 || tlm_port > 0xFFFF)
    {
        mlog(CRITICAL, "Invalid port number for telemetry port: %ld\n", tlm_port);
        return NULL;
    }
    else if(cmd_port < 0 || cmd_port > 0xFFFF)
    {
        mlog(CRITICAL, "Invalid port number for command port: %ld\n", cmd_port);
        return NULL;
    }

    /* Set Max Connections */
    long max_connections = DEFAULT_MAX_CONNECTIONS;
    if(argc > 6)
    {
        if(!StringLib::str2long(argv[6], &max_connections))
        {
            mlog(CRITICAL, "Invalid value provided for max connections: %s\n", argv[6]);
            return NULL;
        }
    }

    /* Create Interface */
    return new CosmosInterface(cmd_proc, name, tlmq_name, cmdq_name, tlm_ip, tlm_port, cmd_ip, cmd_port, (int)max_connections);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CosmosInterface::CosmosInterface(CommandProcessor* cmd_proc, const char* obj_name, const char* tlmq_name, const char* cmdq_name, const char* tlm_ip, int tlm_port, const char* cmd_ip, int cmd_port, int max_connections):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    assert(tlmq_name);
    assert(cmdq_name);
    assert(tlm_ip);
    assert(cmd_ip);

    interfaceActive = true;
    maxConnections = max_connections;

    /* Telemetry Connection Initialization */
    tlmQName = StringLib::duplicate(tlmq_name);
    tlmListener.ci = this;
    tlmListener.ip_addr = StringLib::duplicate(tlm_ip);
    tlmListener.port = tlm_port;
    tlmListener.handler = tlmActiveHandler;
    tlmListenerPid = new Thread(listenerThread, &tlmListener);

    /* Command Connection Initialization */
    cmdQName = StringLib::duplicate(cmdq_name);
    cmdListener.ci = this;
    cmdListener.ip_addr = StringLib::duplicate(cmd_ip);
    cmdListener.port = cmd_port;
    cmdListener.handler = cmdActiveHandler;
    cmdListenerPid = new Thread(listenerThread, &cmdListener);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CosmosInterface::~CosmosInterface(void)
{
    interfaceActive = false;

    delete tlmListenerPid;
    delete cmdListenerPid;

    if(tlmListener.ip_addr) delete [] tlmListener.ip_addr;
    if(cmdListener.ip_addr) delete [] cmdListener.ip_addr;

    delete [] tlmQName;
    delete [] cmdQName;

    tlmConnMut.lock();
    {
        tlmConnections.flush();
        cmdConnections.flush();
    }
    tlmConnMut.unlock();
}

/*----------------------------------------------------------------------------
 * listenerThread  - listener thread for CosmosInterface class
 *----------------------------------------------------------------------------*/
void* CosmosInterface::listenerThread (void* parm)
{
    assert(parm != NULL);

    listener_t* l = (listener_t*)parm;

    int status = SockLib::startserver(l->ip_addr, l->port, l->ci->maxConnections, pollHandler, l->handler, &l->ci->interfaceActive, (void*)l->ci);

    if(status < 0)
    {
        mlog(CRITICAL, "Failed to establish server: %s\n", l->ci->getName());
        l->ci->cmdProc->deleteObject(l->ci->getName());
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * pollHandler  -
 *
 *   Notes: provides the flags back to the poll function
 *----------------------------------------------------------------------------*/
int CosmosInterface::pollHandler(int fd, short* events, void* parm)
{
    (void)fd;
    (void)events;
    (void)parm;

    return 0;
}

/*----------------------------------------------------------------------------
 * tlmActiveHandler  -
 *
 *   Notes: performed on activity returned from poll
 *----------------------------------------------------------------------------*/
int CosmosInterface::tlmActiveHandler(int fd, int flags, void* parm)
{
    CosmosInterface* ci = (CosmosInterface*)parm;

    if(flags & IO_CONNECT_FLAG)
    {
        /* Create Request Info Structure */
        tlm_t* rqst = new tlm_t;
        rqst->ci = ci;
        rqst->sub = new Subscriber(ci->tlmQName);
        rqst->sock = new TcpSocket(NULL, fd);
        mlog(CRITICAL, "Establishing new connection to %s:%d in %s\n", rqst->sock->getIpAddr() ? rqst->sock->getIpAddr() : "UNKNOWN", rqst->sock->getPort(), ci->getName());

        /* Register and Start Connection */
        ci->tlmConnMut.lock();
        {
            ci->tlmConnections.add(rqst->sock->getUniqueId(), rqst);
            rqst->pid = new Thread(telemetryThread, rqst, false);
        }
        ci->tlmConnMut.unlock();
    }

    return 0; // return success
}

/*----------------------------------------------------------------------------
 * cmdActiveHandler  -
 *
 *   Notes: performed on activity returned from poll
 *----------------------------------------------------------------------------*/
int CosmosInterface::cmdActiveHandler(int fd, int flags, void* parm)
{
    CosmosInterface* ci = (CosmosInterface*)parm;

    if(flags & IO_CONNECT_FLAG)
    {
        /* Create Request Info Structure */
        cmd_t* rqst = new cmd_t;
        rqst->ci = ci;
        rqst->pub = new Publisher(ci->cmdQName);
        rqst->sock = new TcpSocket(NULL, fd);
        mlog(CRITICAL, "Establishing new connection to %s:%d in %s\n", rqst->sock->getIpAddr() ? rqst->sock->getIpAddr() : "UNKNOWN", rqst->sock->getPort(), ci->getName());

        /* Register and Start Connection */
        ci->cmdConnMut.lock();
        {
            ci->cmdConnections.add(rqst->sock->getUniqueId(), rqst);
            rqst->pid = new Thread(commandThread, rqst, false);
        }
        ci->cmdConnMut.unlock();
    }

    return 0; // return success
}

/*----------------------------------------------------------------------------
 * telemetryThread  -
 *----------------------------------------------------------------------------*/
void* CosmosInterface::telemetryThread (void* parm)
{
    tlm_t* rqst = (tlm_t*)parm;
    CosmosInterface* ci = rqst->ci;

    unsigned char* buffer = new unsigned char[MAX_PACKET_SIZE + HEADER_SIZE];
    LocalLib::copy(buffer, SYNC_PATTERN, SYNC_SIZE);

    while(ci->interfaceActive)
    {
        /* ReceivePacket */
        int bytes = rqst->sub->receiveCopy(&buffer[HEADER_SIZE], MAX_PACKET_SIZE, SYS_TIMEOUT);
        if(bytes > 0)
        {
            buffer[SYNC_SIZE] = (bytes + HEADER_SIZE) & 0xFF;
            buffer[SYNC_SIZE + 1] = ((bytes + HEADER_SIZE) >> 8) & 0xFF;
            int bytes_sent = rqst->sock->writeBuffer(buffer, bytes + HEADER_SIZE);
            if(bytes_sent != bytes + HEADER_SIZE)
            {
                mlog(CRITICAL, "Message of size %d unable to be sent (%d) to remote destination %s\n", bytes, bytes_sent, rqst->sock->getIpAddr());
                break;
            }
        }
        else if(bytes != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Fatal error (%d) detected trying to read telemetry from %s, exiting telemetry thread in %s\n", bytes, rqst->sub->getName(), ci->getName());
            break;
        }
    }

    mlog(INFO, "Terminating connection to %s in %s\n", rqst->sock->getIpAddr(), ci->getName());
    ci->tlmConnMut.lock();
    {
        // !!! CANNOT access request values after this call !!!
        // this will call freeConnectionEntry which
        // closes the socket and deletes objects
        ci->tlmConnections.remove(rqst->sock->getUniqueId());
    }
    ci->tlmConnMut.unlock();

    delete[] buffer;

    return NULL;
}

/*----------------------------------------------------------------------------
 * commandThread  -
 *----------------------------------------------------------------------------*/
void* CosmosInterface::commandThread (void* parm)
{
    cmd_t* c = (cmd_t*)parm;
    unsigned char header_buf[HEADER_SIZE];
    int header_index = 0;
    int packet_index = 0;
    int packet_size = 0;

    unsigned char* packet_buf = new unsigned char[MAX_PACKET_SIZE];

    while(c->ci->interfaceActive)
    {
        /* Read Header */
        if(header_index != HEADER_SIZE)
        {
            int bytes = c->sock->readBuffer(&header_buf[header_index], HEADER_SIZE - header_index);
            if(bytes > 0)
            {
                header_index += bytes;
                if(header_index == HEADER_SIZE)
                {
                    bool in_sync = true;

                    /* Calculate Packet Size */
                    packet_size = header_buf[LENGTH_OFFSET] + (header_buf[LENGTH_OFFSET + 1] * 256);
                    packet_size -= HEADER_SIZE;
                    if(packet_size < 0)
                    {
                        in_sync = false;
                    }

                    /* Check Synchronization Pattern */
                    for(int i = SYNC_OFFSET; i < SYNC_SIZE; i++)
                    {
                        if(header_buf[i] != SYNC_PATTERN[i])
                        {
                            in_sync = false;
                            break;
                        }
                    }

                    /* Handle Loss of Synchronization */
                    if(!in_sync)
                    {
                        mlog(CRITICAL, "Lost synchronization to COSMOS command interface in %s\n", c->ci->getName());
                        LocalLib::move(&header_buf[0], &header_buf[1], HEADER_SIZE - 1);
                        header_index--;  // shift down
                    }
                }
            }
            else if(!c->sock->isConnected())
            {
                LocalLib::sleep(1);
            }
            else if(bytes != TIMEOUT_RC)
            {
                mlog(CRITICAL, "Failed to read header (%d) on %s command socket... fatal error, exiting command thread\n", bytes, c->ci->getName());
                break;
            }
        }
        /* Read Packet */
        else if(packet_index < packet_size)
        {
            int bytes = c->sock->readBuffer(&packet_buf[packet_index], packet_size - packet_index);
            if(bytes > 0)
            {
                packet_index += bytes;
                if(packet_index == packet_size)
                {
                    /* Post Packet */
                    int status = MsgQ::STATE_TIMEOUT;
                    while(c->ci->interfaceActive && status == MsgQ::STATE_TIMEOUT)
                    {
                        status = c->pub->postCopy(&packet_buf[0], packet_size, SYS_TIMEOUT);
                        if(status < 0 && status != MsgQ::STATE_TIMEOUT)
                        {
                            mlog(CRITICAL, "Message of size %d unable to be posted (%d) to output stream %s\n", bytes, status, c->pub->getName());
                            break;
                        }
                    }

                    /* Clear Control Variables */
                    header_index = 0;
                    packet_index = 0;
                    packet_size = 0;
                }
            }
            else if(!c->sock->isConnected())
            {
                LocalLib::sleep(1);
            }
            else if(bytes != TIMEOUT_RC)
            {
                mlog(CRITICAL, "Failed to read packet (%d) on %s command socket... fatal error, exiting command thread\n", bytes, c->ci->getName());
                break;
            }
        }
    }

    delete[] packet_buf;

    return NULL;
}
