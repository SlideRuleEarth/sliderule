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

#include "DatasrvSocketReader.h"

#include "core.h"
#include "ccsds.h"
#include "legacy.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* DatasrvSocketReader::TYPE = "DatasrvSocketReader";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *
 *   Notes: Creates a request to datasrv to play data. The Retrieve CCSDS Pkt request
 *          message is as follows:
 *
 *            uint8_t sz_of_req_id_str "RTRV_CCSDS_PKTS" (Retrieve ccsds pkts))
 *            chars req_id_str
 *            uint8_t sz_of_name
 *            chars name of archive
 *            uint8_t sz_of_start_time
 *            chars start_time in form <YYYY[MM[DD[HH[MM[SS]]]]]>
 *            uint8_t sz_of_end_time
 *            chars end_time  in form <YYYY[MM[DD[HH[MM[SS]]]]]>
 *            uint8_t sz_of_hdr_on_off
 *            chars [['hdr_on']/['hdr_off']]
 *            uint8_t sz_of_rate
 *            chars rate (0=means as fast as possible 1=realtime, 2-255 mean "<n> times realtime")
 *            uint8_t sz_of_num_apids
 *            chars num_of_apids
 *            uint8_t sz_of_apid_0
 *            chars apid_0
 *            uint8_t sz_of_apid_1
 *            chars apid_1
 *            uint8_t sz_of_apid_2
 *            chars apid_2
 *                .
 *                .
 *                .
 *            uint8_t sz_of_apid_<n-1>
 *            chars apid_<n-1>
 *
 *----------------------------------------------------------------------------*/
DatasrvSocketReader::DatasrvSocketReader(CommandProcessor* cmd_proc, const char* obj_name, const char* _ip_addr, int _port, const char* outq_name, const char* start_time, const char* end_time, const char* req_arch_str, uint16_t apids[], int num_apids):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    assert(outq_name);

    /* Initialize Parameters */
    read_active = true;
    ip_addr = StringLib::duplicate(_ip_addr);
    port = _port;

    /* Output Stream */
    outq = new Publisher(outq_name);

    /* Read Counter */
    bytes_read = 0;

    /* Register Commands */
    registerCommand("LOG_PKT_STATS", (cmdFunc_t)&DatasrvSocketReader::logPktStatsCmd, 0, "");

    /* Build Request String */
    #define MAX_REQ_SIZE            65536
    #define MAX_REQ_APIDS           32
    #define REQ_NUM_APIDS_STR_SIZE  10
    #define REQ_ID_STR              "RTRV_CCSDS_PKTS"
    #define REQ_HDR_STR             "hdr_off"
    #define REQ_RATE_STR            "0"
    #define NUM_APIDS_STR_SIZE      6
    #define APID_STR_SIZE           7

    List<unsigned char> rqst;

    addRqstParm(rqst, REQ_ID_STR);
    addRqstParm(rqst, req_arch_str);
    addRqstParm(rqst, start_time);
    addRqstParm(rqst, end_time);
    addRqstParm(rqst, REQ_HDR_STR);
    addRqstParm(rqst, REQ_RATE_STR);
    addRqstParm(rqst, REQ_ID_STR);

    char num_apids_str[NUM_APIDS_STR_SIZE];
    snprintf(num_apids_str, NUM_APIDS_STR_SIZE, "%d", num_apids);
    addRqstParm(rqst, num_apids_str);

    if(apids != NULL)
    {
        for(int t = 0; t < num_apids; t++)
        {
            char apid_str[APID_STR_SIZE];
            snprintf(apid_str, APID_STR_SIZE, "0x%04X", apids[t]);
            addRqstParm(rqst, apid_str);
        }
    }

    /* Set Class Request Member */
    request_size = rqst.length();
    request = new unsigned char[request_size];
    for(int i = 0; i < request_size; i++) request[i] = rqst[i];

    /* Start Reader Thread */
    reader = new Thread(reader_thread, this);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
DatasrvSocketReader::~DatasrvSocketReader(void)
{
    read_active = false;
    delete reader;

    delete [] ip_addr;
    delete outq;

    delete request;
}

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* DatasrvSocketReader::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    /* Check Number of Arguments */
    int minargs = 6;
    if(minargs > argc)
    {
        mlog(CRITICAL, "not enough parameters supplied: %d\n", argc);
        return NULL;
    }

    /* Parse Out Parameters */
    char*       ip_addr      = argv[0];
    int         port         = (int)strtol(argv[1], NULL, 0);
    const char* outq         = StringLib::checkNullStr(argv[2]);
    const char* start        = argv[3];
    const char* end          = argv[4];
    const char* req_arch_str = argv[5];

    /* Set Apids */
    uint16_t* apids = NULL;
    int num_apids = argc - minargs;
    if(num_apids > 0)
    {
        apids = new uint16_t[num_apids];
        for(int i = 0; i < num_apids; i++)
        {
            apids[i] = (unsigned int)strtol(argv[i + minargs], NULL, 0);
        }
    }

    /* Create Datasrv Socket Reader */
    DatasrvSocketReader* reader = new DatasrvSocketReader(cmd_proc, name, ip_addr, port, outq, start, end, req_arch_str, apids, num_apids);

    /* Free Apids */
    if(apids) delete [] apids;

    /* Return Datasrv Socket Reader */
    return reader;
}

/*----------------------------------------------------------------------------
 * parseApidSet  -
 *----------------------------------------------------------------------------*/
int DatasrvSocketReader::parseApidSet(const char* apid_set, uint16_t apids[])
{
    char apid_toks[CCSDS_NUM_APIDS][MAX_STR_SIZE];
    int num_toks = StringLib::tokenizeLine(apid_set, strlen(apid_set) + 1, ' ', CCSDS_NUM_APIDS, apid_toks);

    for(int i = 0; i < num_toks; i++)
    {
        apids[i] = (uint16_t)strtol(apid_toks[i], NULL, 0);
    }

    return num_toks;
}

/******************************************************************************
 * PRIVATE METHODS (STATIC)
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * reader_thread  - reader thread for AdasSocketReader class
 *----------------------------------------------------------------------------*/
void* DatasrvSocketReader::reader_thread (void* parm)
{
    assert(parm != NULL);
    DatasrvSocketReader* sr = (DatasrvSocketReader*)parm;
    int io_maxsize = LocalLib::getIOMaxsize();
    char* record = new char [io_maxsize];

    /* Connect to Datasrv */
    TcpSocket* sock = new TcpSocket(NULL, sr->ip_addr, sr->port, false, &sr->read_active, true); // blocking

    /* Initialize Connection */
    if(sock->isConnected())
    {
        sr->initConnection(sock);
    }
    else
    {
        mlog(CRITICAL, "Unable to establish connection to datasrv: %s\n", sr->getName());
        sr->read_active = false;
        sr->cmdProc->deleteObject(sr->getName());
    }

    /* Read Data from Datasrv */
    while(sr->read_active)
    {
        /* Block on Reading Socket */
        int bytes = 0;
        if((bytes = sock->readBuffer(record, io_maxsize)) > 0)
        {
            int status = sr->outq->postCopy(record, bytes, SYS_TIMEOUT);
            if(status > 0)
            {
                sr->bytes_read += bytes;
            }
            else if(status != MsgQ::STATE_TIMEOUT)
            {
                mlog(ERROR, "Data server reader %s unable to post to stream %s (%d)\n", sr->getName(), sr->outq->getName(), status);
            }
        }
        else if(bytes < 0)
        {
            mlog(CRITICAL, "%s failed to read from socket, ... closing connection and exiting reader!\n", sr->getName());
            sr->read_active = false; // breaks out of loop
            sr->cmdProc->deleteObject(sr->getName());
        }
        else
        {
            mlog(CRITICAL, "Data server reader %s closing connection\n", sr->getName());
            sr->read_active = false;
        }
    }

    /* Exit Reader Thread */
    delete [] record;
    return NULL;
}

/******************************************************************************
 * PRIVATE METHODS (NON-STATIC)
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * initConnection  - initialize connection hook
 *----------------------------------------------------------------------------*/
bool DatasrvSocketReader::initConnection (TcpSocket* sock)
{
    bool initialized = false;

    mlog(DEBUG, "Sending archive request to datasrv...\n");

    int sent_bytes = sock->writeBuffer(request, request_size);
    if(sent_bytes == request_size)
    {
        mlog(CRITICAL, "Connection established to datasrv at %s:%d\n", sock->getIpAddr(), sock->getPort());
        initialized = true;
    }
    else
    {
        mlog(CRITICAL, "Unable to send request packet to datasrv: %d - %s\n", sent_bytes, strerror(errno));
    }

    return initialized;
}

/*----------------------------------------------------------------------------
 * logPktStatsCmd  -
 *----------------------------------------------------------------------------*/
int DatasrvSocketReader::logPktStatsCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    mlog(RAW, "\n");
    mlog(RAW, "Datasrv Reader %s: %lu\n", getName(), bytes_read);
    mlog(RAW, "\n");

    return 0;
}

/*----------------------------------------------------------------------------
 * addRqstParm  -
 *----------------------------------------------------------------------------*/
void DatasrvSocketReader::addRqstParm (List<unsigned char>& l, const char* parm)
{
    unsigned char str_len = (unsigned char)strlen(parm);
    l.add(str_len);

    for(unsigned int i = 0; i < strlen(parm); i++)
    {
        unsigned char str_char = (unsigned char)parm[i];
        l.add(str_char);
    }
}
