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

#include "AdasSocketReader.h"

#include "core.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* AdasSocketReader::TYPE = "AdasSocketReader";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* AdasSocketReader::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* ip_addr = argv[0];
    const char* port_str = argv[1];
    const char* outq    = StringLib::checkNullStr(argv[2]);

    long port = 0;
    if(!StringLib::str2long(port_str, &port))
    {
        mlog(CRITICAL, "Invalid port supplied: %s\n", port_str);
        return NULL;
    }

    return new AdasSocketReader(cmd_proc, name, ip_addr, port, outq);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
AdasSocketReader::AdasSocketReader(CommandProcessor* cmd_proc, const char* obj_name, const char* ip_addr, int port, const char* outq_name):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    assert(outq_name);

    /* Initialize Parameters */
    read_active = true;

    /* Create Socket */
    sock = new TcpSocket(NULL, ip_addr, port, false, NULL, false);

    /* Output Stream */
    outq = new Publisher(outq_name);

    /* Read Counter */
    bytes_read = 0;

    /* Register Commands */
    registerCommand("LOG_PKT_STATS", (cmdFunc_t)&AdasSocketReader::logPktStatsCmd, -1, "[<APID>]");

    /* Start Reader Thread */
    reader = new Thread(reader_thread, this);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
AdasSocketReader::~AdasSocketReader(void)
{
    read_active = false;
    delete reader;

    delete sock;
    delete outq;
}

/*----------------------------------------------------------------------------
 * reader_thread  - reader thread for AdasSocketReader class
 *----------------------------------------------------------------------------*/
void* AdasSocketReader::reader_thread (void* parm)
{
    bool connection_initialized = false;

    assert(parm != NULL);

    AdasSocketReader* sr = (AdasSocketReader*)parm;

    int io_maxsize = LocalLib::getIOMaxsize();
    char* record = new char [io_maxsize];

    while(sr->read_active)
    {
        /* Poll For Connection */
        if(!sr->sock->isConnected())
        {
            mlog(ERROR, "ADAS socket not connected... sleeping - %s\n", sr->getName());
            LocalLib::sleep(1);
            continue;
        }
        else if(!connection_initialized)
        {
            if(sr->initConnection())
            {
                connection_initialized = true;
            }
            else
            {
                mlog(ERROR, "ADAS socket not initialized... closing and retrying - %s\n", sr->getName());
                sr->sock->closeConnection();
                continue;
            }
        }

        /* Block on Reading Socket */
        int bytes = 0;
        if((bytes = sr->sock->readBuffer(record, io_maxsize)) > 0)
        {
            if(sr->outq->postCopy(record, bytes) <= 0)
            {
                mlog(ERROR, "ADAS socket reader %s unable to post to stream %s\n", sr->getName(), sr->outq->getName());
            }
            else
            {
                sr->bytes_read += bytes;
            }
        }
        else
        {
            mlog(CRITICAL, "%s failed to read from socket, ... attempting to re-establish connection!\n", sr->getName());
            sr->sock->closeConnection();
            connection_initialized = false;
        }
    }

    /* Close Socket Connection */
    delete [] record;
    return NULL;
}

/*----------------------------------------------------------------------------
 * logPktStatsCmd  -
 *----------------------------------------------------------------------------*/
int AdasSocketReader::logPktStatsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    mlog(RAW, "\n");
    mlog(RAW, "ADAS Socket Reader %s: %d\n", getName(), bytes_read);
    mlog(RAW, "\n");

    return 0;
}

/*----------------------------------------------------------------------------
 * initConnection  - initialize connection hook
 *----------------------------------------------------------------------------*/

bool AdasSocketReader::initConnection (void)
{
    bool initialized = false;

    const char* request_pkt = "CCSD3ZA0000100000022C7333IA0SFID0000000270";
    int request_len = (int)strlen(request_pkt);
    mlog(DEBUG, "Sending ID request packet to ADAS\n");

    int sent_bytes = sock->writeBuffer((unsigned char*)request_pkt, request_len);
    if(sent_bytes == request_len)
    {
        #define MAX_RESPONSE_SIZE   100
        char response_pkt[MAX_RESPONSE_SIZE];
        memset(response_pkt, 0, MAX_RESPONSE_SIZE);
        mlog(DEBUG, "Pending on receive of ACK response packet from ADAS\n");

        int recv_bytes = sock->readBuffer(response_pkt, MAX_RESPONSE_SIZE);
        if(recv_bytes > 0 && recv_bytes < MAX_RESPONSE_SIZE)
        {
            if(strncmp(response_pkt, "CCSD3ZA0000100000023C7333IA0AKNK00000003ACK", MAX_RESPONSE_SIZE) == 0)
            {
                mlog(CRITICAL, "Connection established to ADAS at %s:%d\n", sock->getIpAddr(), sock->getPort());
                initialized = true;
            }
        }
        else
        {
            mlog(CRITICAL, "Unable to response ACK packet(%d) from ADAS... fatal error, waiting 5 seconds!\n", recv_bytes);
        }
    }
    else
    {
        mlog(CRITICAL, "Unable to send ID packet(%d) to ADAS... fatal error, waiting 5 seconds!\n", sent_bytes);
    }

    return initialized;
}
