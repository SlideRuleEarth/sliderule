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

#include "CfsInterface.h"
#include "CommandProcessor.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CfsInterface::TYPE = "CfsInterface";

/******************************************************************************
 * PACKET STATISTIC RECORD SUBCLASS
 ******************************************************************************/

const char* CfsInterface::PktStats::rec_type = "CfsPktStat";

RecordObject::fieldDef_t CfsInterface::PktStats::rec_def[] =
{
    {"APID",            UINT32, offsetof(pktStats_t, apid),             sizeof(((pktStats_t*)0)->apid),              NATIVE_FLAGS},
    {"SEGS_READ",       UINT32, offsetof(pktStats_t, segs_read),        sizeof(((pktStats_t*)0)->segs_read),         NATIVE_FLAGS},
    {"SEGS_ERRORS",     UINT32, offsetof(pktStats_t, segs_errors),      sizeof(((pktStats_t*)0)->segs_errors),       NATIVE_FLAGS},
    {"SEGS_FORWARDED",  UINT32, offsetof(pktStats_t, segs_forwarded),   sizeof(((pktStats_t*)0)->segs_forwarded),    NATIVE_FLAGS},
    {"SEGS_DROPPED",    UINT32, offsetof(pktStats_t, segs_dropped),     sizeof(((pktStats_t*)0)->segs_dropped),      NATIVE_FLAGS},
    {"TOTAL_BYTES",     UINT32, offsetof(pktStats_t, total_bytes),      sizeof(((pktStats_t*)0)->total_bytes),       NATIVE_FLAGS},
    {"TOTAL_PKTS",      UINT32, offsetof(pktStats_t, total_pkts),       sizeof(((pktStats_t*)0)->total_pkts),        NATIVE_FLAGS},
    {"SEQ_ERRORS",      UINT32, offsetof(pktStats_t, seq_errors),       sizeof(((pktStats_t*)0)->seq_errors),        NATIVE_FLAGS},
    {"SEG_ERRORS",      UINT32, offsetof(pktStats_t, seg_errors),       sizeof(((pktStats_t*)0)->seg_errors),        NATIVE_FLAGS},
    {"LEN_ERRORS",      UINT32, offsetof(pktStats_t, len_errors),       sizeof(((pktStats_t*)0)->len_errors),        NATIVE_FLAGS},
    {"CHKSUM_ERRORS",   UINT32, offsetof(pktStats_t, chksum_errors),    sizeof(((pktStats_t*)0)->chksum_errors),     NATIVE_FLAGS},
    {"FILTER_FACTOR",   UINT32, offsetof(pktStats_t, filter_factor),    sizeof(((pktStats_t*)0)->filter_factor),     NATIVE_FLAGS},
    {"AVG_BPS",         DOUBLE, offsetof(pktStats_t, avg_bps),          sizeof(((pktStats_t*)0)->avg_bps),           NATIVE_FLAGS}
};

int CfsInterface::PktStats::rec_elem = sizeof(CfsInterface::PktStats::rec_def) / sizeof(RecordObject::fieldDef_t);

CfsInterface::PktStats::PktStats(CommandProcessor* cmd_proc, const char* stat_name): StatisticRecord<pktStats_t>(cmd_proc, stat_name, rec_type)
{
    cmdProc->registerObject(stat_name, this);
}

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* CfsInterface::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    /* Parse Inputs */
    const char*     tlmq_name       = StringLib::checkNullStr(argv[0]);
    const char*     cmdq_name       = StringLib::checkNullStr(argv[1]);
    const char*     tlm_ip          = StringLib::checkNullStr(argv[2]);
    const char*     tlm_port_str    = StringLib::checkNullStr(argv[3]);
    const char*     cmd_ip          = StringLib::checkNullStr(argv[4]);
    const char*     cmd_port_str    = StringLib::checkNullStr(argv[5]);

    long            tlm_port        = 0;
    long            cmd_port        = 0;

    if(tlmq_name)
    {
        if(!StringLib::str2long(tlm_port_str, &tlm_port))
        {
            mlog(CRITICAL, "Invalid value provided for telemetry port: %s\n", tlm_port_str);
            return NULL;
        }
        else if(tlm_port < 0 || tlm_port > 0xFFFF)
        {
            mlog(CRITICAL, "Invalid port number for telemetry port: %ld\n", tlm_port);
            return NULL;
        }
    }

    if(cmdq_name)
    {
        if(!StringLib::str2long(cmd_port_str, &cmd_port))
        {
            mlog(CRITICAL, "Invalid value provided for command port: %s\n", cmd_port_str);
            return NULL;
        }
        else if(cmd_port < 0 || cmd_port > 0xFFFF)
        {
            mlog(CRITICAL, "Invalid port number for command port: %ld\n", cmd_port);
            return NULL;
        }
    }

    /* Create Interface */
    return new CfsInterface(cmd_proc, name, tlmq_name, cmdq_name, tlm_ip, tlm_port, cmd_ip, cmd_port);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CfsInterface::CfsInterface(CommandProcessor* cmd_proc, const char* obj_name, const char* tlmq_name, const char* cmdq_name, const char* tlm_ip, int tlm_port, const char* cmd_ip, int cmd_port):
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Define Packet Statistics Record */
    PktStats::defineRecord(PktStats::rec_type, "APID", sizeof(pktStats_t), PktStats::rec_def, PktStats::rec_elem, 32);

    /* Initialize APID Statistics */
    LocalLib::set(apidStats, 0, sizeof(apidStats));
    apidStats[CMD_APIDS] = createPktStat(CMD_APIDS);
    apidStats[TLM_APIDS] = createPktStat(TLM_APIDS);

    /* Initialize Attributes */
    interfaceActive = true;
    dropInvalidPkts = false;

    /* Create Telemetry Processing */
    if(tlmq_name)
    {
        tlmSock = new UdpSocket(NULL, tlm_ip, tlm_port, true, NULL); // receiving socket
        tlmQ = new Publisher(tlmq_name);
        telemetryPid = new Thread(telemetryThread, this);
    }
    else
    {
        tlmSock = NULL;
        tlmQ = NULL;
        telemetryPid = NULL;
    }

    /* Create Command Processing */
    if(cmdq_name)
    {
        cmdSock = new UdpSocket(NULL, cmd_ip, cmd_port, false, NULL); // sending socket
        cmdQ = new Subscriber(cmdq_name);
        commandPid = new Thread(commandThread, this);
    }
    else
    {
        cmdSock = NULL;
        cmdQ = NULL;
        commandPid = NULL;
    }

    /* Register Commands */
    registerCommand("DROP_INVALID",  (cmdFunc_t)&CfsInterface::dropInvalidCmd,    1, "<TRUE|FALSE>");
    registerCommand("LOG_PKT_STATS", (cmdFunc_t)&CfsInterface::logPktStatsCmd,   -1, "[<apid|TLM|CMD> ...]");
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CfsInterface::~CfsInterface(void)
{
    interfaceActive = false;
    if(telemetryPid) delete telemetryPid;
    if(commandPid) delete commandPid;

    if(tlmSock) delete tlmSock;
    if(cmdSock) delete cmdSock;

    if(tlmQ) delete tlmQ;
    if(cmdQ) delete cmdQ;

    for(int i = 0; i < CCSDS_NUM_APIDS + 1; i++)
    {
        if(apidStats[i] != NULL)
        {
            cmdProc->deleteObject(apidStats[i]->getName());
        }
    }
}

/*----------------------------------------------------------------------------
 * telemetryThread  -
 *----------------------------------------------------------------------------*/
void* CfsInterface::telemetryThread (void* parm)
{
    CfsInterface* interface = (CfsInterface*)parm;
    unsigned char* buffer = new unsigned char[CCSDS_MAX_SPACE_PACKET_SIZE];

    while(interface->interfaceActive)
    {
        /* Read Packet */
        int bytes = interface->tlmSock->readBuffer(buffer, CCSDS_MAX_SPACE_PACKET_SIZE);
        if(bytes > CCSDS_SPACE_HEADER_SIZE)
        {
            /* Create Packet Statistic (If Necessary) */
            int apid = CCSDS_GET_APID(buffer);
            if(interface->apidStats[apid] == NULL) // TODO: Needs to be mutexed... not safe with commandThread
            {
                interface->apidStats[apid] = interface->createPktStat(apid); // TODO: put mutex inside the createPktStat function
            }

            /* Validate Packet */
            bool valid = interface->validatePkt(buffer, bytes);
            if(valid)
            {
                interface->apidStats[apid]->rec->segs_read++;
                interface->apidStats[TLM_APIDS]->rec->segs_read++;
            }
            else
            {
                mlog(WARNING, "Invalid packet (unknown) detected in %s telemetry\n", interface->getName());
                interface->apidStats[apid]->rec->segs_errors++;
                interface->apidStats[TLM_APIDS]->rec->segs_errors++;
            }

            /* Attempt to Post Packet */
            if(valid || !interface->dropInvalidPkts)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(interface->interfaceActive && status == MsgQ::STATE_TIMEOUT)
                {
                    status = interface->tlmQ->postCopy(buffer, bytes, SYS_TIMEOUT);
                    if(status > 0)
                    {
                        interface->measurePkt(buffer, bytes);
                        interface->apidStats[apid]->rec->segs_forwarded++;
                        interface->apidStats[TLM_APIDS]->rec->segs_forwarded++;
                    }
                    else if(status != MsgQ::STATE_TIMEOUT)
                    {
                        mlog(CRITICAL, "Packet (SID = 0x%04X) unable to be posted (%d) to output stream %s\n", CCSDS_GET_SID(buffer), status, interface->tlmQ->getName());
                        interface->apidStats[apid]->rec->segs_dropped++;
                        interface->apidStats[TLM_APIDS]->rec->segs_dropped++;
                        break;
                    }
                }
            }
        }
        else if(bytes != TIMEOUT_RC)
        {
            mlog(CRITICAL, "Failed to read packet (%d) on %s telemetry socket... fatal error, exiting telemetry thread\n", bytes, interface->getName());
            break;
        }
    }

    delete[] buffer;

    return NULL;
}

/*----------------------------------------------------------------------------
 * commandThread  -
 *----------------------------------------------------------------------------*/
void* CfsInterface::commandThread (void* parm)
{
    CfsInterface* interface = (CfsInterface*)parm;
    unsigned char buffer[CCSDS_MAX_SPACE_PACKET_SIZE];

    while(interface->interfaceActive)
    {
        /* ReceivePacket */
        int bytes = interface->cmdQ->receiveCopy(buffer, CCSDS_MAX_SPACE_PACKET_SIZE, SYS_TIMEOUT);
        if(bytes > CCSDS_SPACE_HEADER_SIZE)
        {
            /* Create Packet Statistic (If Necessary) */
            int apid = CCSDS_GET_APID(buffer);
            if(interface->apidStats[apid] == NULL) // TODO: Needs to be mutexed... not safe with telemetryThread
            {
                interface->apidStats[apid] = interface->createPktStat(apid);
            }

            /* Validate Packet */
            bool valid = interface->validatePkt(buffer, bytes);
            if(valid)
            {
                interface->apidStats[apid]->rec->segs_read++;
                interface->apidStats[CMD_APIDS]->rec->segs_read++;
            }
            else
            {
                mlog(CRITICAL, "Invalid packet %0X4 detected in %s commands\n", apid, interface->getName());
                interface->apidStats[apid]->rec->segs_errors++;
                interface->apidStats[CMD_APIDS]->rec->segs_errors++;
            }

            /* Attempt to Send Packet */
            if(valid || !interface->dropInvalidPkts)
            {
                int bytes_sent = interface->cmdSock->writeBuffer(buffer, bytes);
                if(bytes_sent == bytes)
                {
                    interface->measurePkt(buffer, bytes);
                    interface->apidStats[apid]->rec->segs_forwarded++;
                    interface->apidStats[CMD_APIDS]->rec->segs_forwarded++;
                }
                else
                {
                    mlog(CRITICAL, "Packet (SID = 0x%04X) unable to be sent (%d) to remote destination %s\n", CCSDS_GET_SID(buffer), bytes_sent, interface->cmdSock->getIpAddr());
                    interface->apidStats[apid]->rec->segs_dropped++;
                    interface->apidStats[CMD_APIDS]->rec->segs_dropped++;
                }
            }
        }
        else if(bytes < 0)
        {
            mlog(CRITICAL, "Fatal error (%d) detected trying to read commands from %s, exiting command thread in %s\n", bytes, interface->cmdQ->getName(), interface->getName());
            return NULL;
        }
        else if(bytes != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "Invalid packet length of %d detected in %s commands\n", bytes, interface->getName());
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * passInvalidCmd
 *----------------------------------------------------------------------------*/
int CfsInterface::dropInvalidCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool drop_invalid;
    if(!StringLib::str2bool(argv[0], &drop_invalid))
    {
        mlog(CRITICAL, "Invalid boolean passed to command: %s\n", argv[0]);
        return -1;
    }

    dropInvalidPkts = drop_invalid;

    return 0;
}

/*----------------------------------------------------------------------------
 * logPktStatsCmd
 *----------------------------------------------------------------------------*/
int CfsInterface::logPktStatsCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    for(int i = 0; i < argc; i++)
    {
        /* Parse APID */
        char* apid_str = argv[0];
        long apid = -1;
        if(StringLib::match(apid_str, "CMD") || StringLib::match(apid_str, "cmd"))
        {
            apid = CMD_APIDS;
        }
        else if(StringLib::match(apid_str, "TLM") || StringLib::match(apid_str, "tlm"))
        {
            apid = TLM_APIDS;
        }
        else if(!StringLib::str2long(apid_str, &apid))
        {
            mlog(CRITICAL, "Invalid APID string supplied: %s\n", apid_str);
            return -1;
        }
        else if(apid < 0 || apid >= CCSDS_NUM_APIDS)
        {
            mlog(CRITICAL, "APID out of range: %ld\n", apid);
            return -1;
        }

        /* Display Statistics */
        if(apidStats[apid])
        {
            mlog(RAW, "--------------------------\n");
            if(apid == CMD_APIDS) mlog(RAW, "COMMANDS\n");
            else if(apid == TLM_APIDS) mlog(RAW, "TELEMETRY\n");
            else mlog(RAW, "APID:            %04X\n", apidStats[apid]->rec->apid);
            mlog(RAW, "--------------------------\n");
            mlog(RAW, "SEGS READ:       %d\n",     apidStats[apid]->rec->segs_read);
            mlog(RAW, "SEGS ERRORS:     %d\n",     apidStats[apid]->rec->segs_errors);
            mlog(RAW, "SEGS FORWARDED:  %d\n",     apidStats[apid]->rec->segs_forwarded);
            mlog(RAW, "SEGS DROPPED:    %d\n",     apidStats[apid]->rec->segs_dropped);
            mlog(RAW, "TOTAL BYTES:     %d\n",     apidStats[apid]->rec->total_bytes);
            mlog(RAW, "TOTAL PKTS:      %d\n",     apidStats[apid]->rec->total_pkts);
            mlog(RAW, "SEQ ERRORS:      %d\n",     apidStats[apid]->rec->seq_errors);
            mlog(RAW, "SEG ERRORS:      %d\n",     apidStats[apid]->rec->seg_errors);
            mlog(RAW, "LEN ERRORS:      %d\n",     apidStats[apid]->rec->len_errors);
            mlog(RAW, "CKS ERRORS:      %d\n",     apidStats[apid]->rec->chksum_errors);
            mlog(RAW, "FILTER:          %d\n",     apidStats[apid]->rec->filter_factor);
            mlog(RAW, "AVG BITS/SEC:    %lf\n",    apidStats[apid]->rec->avg_bps);
        }
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * createPktStat  -
 *----------------------------------------------------------------------------*/
CfsInterface::PktStats* CfsInterface::createPktStat(uint16_t apid)
{
    /* Create StatisticRecord Name */
    char apidStr[5];
    StringLib::format(apidStr, 5, "%03X", apid);
    char* pkt_stats_name = StringLib::concat(PktStats::rec_type, ".", apidStr);

    /* Create StatisticRecord */
    PktStats* pkt_stat = new PktStats(cmdProc, pkt_stats_name);

    /* Free Name */
    delete [] pkt_stats_name;

    /* Set APID - Fixed */
    pkt_stat->rec->apid = apid;

    /* Return StatisticRecord*/
    return pkt_stat;
}

/*----------------------------------------------------------------------------
 * validatePkt  -
 *
 *   Notes: assumes primary header is present
 *----------------------------------------------------------------------------*/
bool CfsInterface::validatePkt (unsigned char* pktbuf, int bytes)
{
    bool status = true;

    try
    {
        /* Create Ccsds Packet */
        CcsdsSpacePacket pkt(pktbuf, bytes);

        /* Get Primary Header Fields */
        unsigned int apid = pkt.getAPID();
        unsigned int seq = pkt.getSEQ();
        unsigned int len = pkt.getLEN();
        seg_t        seg = pkt.getSEQFLG();
        bool         cmd = pkt.isCMD();

        /* Get Statistics */
        pktStats_t*  stat = apidStats[apid]->rec;
        pktStats_t*  all = apidStats[cmd ? CMD_APIDS : TLM_APIDS]->rec;

        /* Command Packet Processing */
        if(cmd)
        {
            if(pkt.validChecksum() == false)
            {
                status = false;
                stat->chksum_errors++;
                all->chksum_errors++;
                mlog(ERROR, "incorrect checksum in command packet 0x%04X: EXPECTED 0x%02X, CHECKSUM 0x%02X\n", apid, pkt.computeChecksum(), pkt.getChecksum());
            }
        }

        /* Telemetry Packet Processing */
        if(!cmd)
        {
            /* Sequence Validation */
            if(stat->total_pkts > 2)
            {
                if(((stat->last_seq + stat->filter_factor) & 0x3FFF) != seq)
                {
                    status = false;
                    stat->seq_errors++;
                    all->seq_errors++;
                    mlog(WARNING, "packet %04X seq %04X unexpected from previous seq %04X (filter factor of %d)\n", apid, seq, stat->last_seq, stat->filter_factor);
                }
            }

            /* Increment Stats */
            if(seq >= stat->last_seq)
            {
                stat->filter_factor = seq - stat->last_seq;
            }
            else
            {
                stat->filter_factor = (0x4000 - stat->last_seq) + seq;
            }
            stat->last_seq = seq;
        }

        /* Length Validation */
        if(len > (unsigned int)bytes)
        {
            status = false;
            stat->len_errors++;
            all->len_errors++;
            mlog(ERROR, "packet %04X failed length validation, exp: %d, act: %d\n", apid, bytes, len);
        }

        /* Segmentation Sequence Validation */
        if((stat->last_seg != CcsdsSpacePacket::SEG_STOP) && (seg == CcsdsSpacePacket::SEG_START))
        {
            mlog(ERROR, "missing \"stop\" segmentation flags for APID %04X SEQ %04X (%02X %02X)\n", apid, seq, stat->last_seg, seg);
            stat->seg_errors++;
            all->seg_errors++;
            status = false;
        }
        else if((stat->last_seg == CcsdsSpacePacket::SEG_STOP) && (seg != CcsdsSpacePacket::SEG_START))
        {
            mlog(ERROR, "missing \"start\" segmentation flags for APID %04X SEQ %04X (%02X %02X)\n", apid, seq, stat->last_seg, seg);
            stat->seg_errors++;
            all->seg_errors++;
            status = false;
        }
        stat->last_seg = seg;
    }
    catch (const std::invalid_argument& e)
    {
        mlog(CRITICAL, "Unable to create or validate CCSDS packet: %s\n", e.what());
        status = false;
    }

    /* Return Status */
    return status;
}

/*----------------------------------------------------------------------------
 * measurePkt  -
 *
 *   Notes: assumes primary header is present
 *----------------------------------------------------------------------------*/
void CfsInterface::measurePkt (unsigned char* pktbuf, int bytes)
{
    if(pktbuf == NULL || bytes < 6) return;

    unsigned int apid = CCSDS_GET_APID(pktbuf);
    int64_t        now  = TimeLib::gettimems();
    bool         cmd  = CCSDS_IS_CMD(pktbuf);
    unsigned int seg  = CCSDS_GET_SEQFLG(pktbuf);

    /* Get Statistics */
    pktStats_t*  stat = apidStats[apid]->rec;
    pktStats_t*  all = apidStats[cmd ? CMD_APIDS : TLM_APIDS]->rec;

    /* Calculate Totals*/
    stat->total_bytes += bytes;
    all->total_bytes += bytes;
    if(seg == CcsdsSpacePacket::SEG_NONE || seg == CcsdsSpacePacket::SEG_STOP)
    {
        stat->total_pkts++;
        all->total_pkts++;
    }

    /* Calculate Packet Average Bits per Second */
    if(stat->first_pkt_time == 0)
    {
        stat->first_pkt_time = TimeLib::gettimems();
    }
    else
    {
        stat->total_pkt_time = now - stat->first_pkt_time;
        stat->avg_bps = (double)stat->total_bytes / (double)(stat->total_pkt_time * 1000);
    }

    /* Calculate Total Average Bits per Second */
    if(all->first_pkt_time == 0)
    {
        all->first_pkt_time = TimeLib::gettimems();
    }
    else
    {
        all->total_pkt_time = now - stat->first_pkt_time;
        all->avg_bps = (double)all->total_bytes / (double)(all->total_pkt_time * 1000);
    }
}
