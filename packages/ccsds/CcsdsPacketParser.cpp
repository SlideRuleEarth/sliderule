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

#include "CcsdsPacketParser.h"
#include "CcsdsParserModule.h"
#include "core.h"

#include <float.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsPacketParser::LuaMetaName = "CcsdsPacketParser";
const struct luaL_Reg CcsdsPacketParser::LuaMetaTable[] = {
    {"passinvalid", luaPassInvalid},
    {"resetinvalid",luaResetInvalid},
    {"stats",       luaLogPktStats},
    {"filter",      luaFilterPkt},
    {"clear",       luaClearApidStats},
    {"striphdr",    luaStripHdrOnPost},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - parser(<parser>, <type - ccsds.ENCAP|ccsds.SPACE>, <inq>, <outq>, <statq>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        CcsdsParserModule*  _parser     = (CcsdsParserModule*)lockLuaObject(L, 1, CcsdsParserModule::OBJECT_TYPE);
        const char*         type_str    = getLuaString(L, 2);
        const char*         inq_name    = getLuaString(L, 3);
        const char*         outq_name   = getLuaString(L, 4, true, NULL);
        const char*         statq_name  = getLuaString(L, 5, true, NULL);

        /* Get Packet Type */
        CcsdsPacket::type_t pkt_type = str2pkttype(type_str);
        if(pkt_type == CcsdsPacket::INVALID_PACKET)
        {
            throw LuaException("invalid packet type: %s", type_str);
        }

        /* Create Packet Parser */
        return createLuaObject(L, new CcsdsPacketParser(L, _parser, pkt_type, inq_name, outq_name, statq_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsPacketParser::CcsdsPacketParser(lua_State* L, CcsdsParserModule* _parser, CcsdsPacket::type_t _type, const char* inq_name, const char* outq_name, const char* statq_name):
    MsgProcessor(L, inq_name, LuaMetaName, LuaMetaTable)
{
    assert(_parser);
    assert(inq_name);

    /* Initialize Attribute Data */
    telemetryWaitSeconds    = 1;
    passInvalid             = false;
    resetInvalid            = false;
    stripHdrOnPost          = false;
    parserInSync            = true;
    parserBytes             = 0;

    /* Set Parser */
    parser = _parser;

    /* Initialize APID Statistics */
    LocalLib::set(&apidStats, 0, sizeof(apidStats));
    for(int i = 0; i <= CCSDS_NUM_APIDS; i++)
    {
        apidStats[i].apid = i;
        apidStats[i].min_bps = DBL_MAX;
    }

    /* Initialize Filter */
    for(int i = 0; i < CCSDS_NUM_APIDS; i++)
    {
        filter[i] = true;
    }

    /* Initialize CCSDS Packet Buffer */
    pktType = _type;
    if(pktType == CcsdsPacket::ENCAPSULATION_PACKET)
    {
        pkt = new CcsdsEncapPacket(CCSDS_MAX_ENCAP_PACKET_SIZE);
    }
    else if(pktType == CcsdsPacket::SPACE_PACKET)
    {
        pkt = new CcsdsSpacePacket(CCSDS_MAX_SPACE_PACKET_SIZE);
    }

    /* Initialize Output Stream */
    outQ = NULL;
    if(outq_name != NULL)
    {
        outQ = new Publisher(outq_name);
    }

    /* Initialize Statistics Stream */
    statQ = NULL;
    if(statq_name != NULL)
    {
        statQ = new Publisher(statq_name);
    }

    /* Create Threads */
    telemetryActive = true;
    telemetryThread = new Thread(telemetry_thread, this);

    /* Start Processor */
    start();
}

/*----------------------------------------------------------------------------
 * Denstructor
 *----------------------------------------------------------------------------*/
CcsdsPacketParser::~CcsdsPacketParser(void)
{
    telemetryActive = false;
    delete telemetryThread;

    stop(); // stop processor

    delete pkt;

    if(outQ)    delete outQ;
    if(statQ)   delete statQ;
}

/*----------------------------------------------------------------------------
 * luaPassInvalid - :passinvalid(<enable>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaPassInvalid (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketParser* lua_obj = (CcsdsPacketParser*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool pass_invalid = getLuaBoolean(L, 2);

        /* Set Pass Invalid */
        lua_obj->passInvalid = pass_invalid;

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting pass invalid state: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaResetInvalid - :rstinvalid(<enable>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaResetInvalid (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketParser* lua_obj = (CcsdsPacketParser*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool reset_invalid = getLuaBoolean(L, 2);

        /* Set Reset Invalid */
        lua_obj->resetInvalid = reset_invalid;

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting pass invalid state: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaLogPktStats - stats(<apid>, [<lvl>])
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaLogPktStats (lua_State* L)
{
    bool status = false;
    int num_obj_to_return = 1;

    try
    {
        /* Get Self */
        CcsdsPacketParser* lua_obj = (CcsdsPacketParser*)getLuaSelf(L, 1);

        /* Get Parameters */
        long        apid    = getLuaInteger(L, 2);
        log_lvl_t   lvl     = (log_lvl_t)getLuaInteger(L, 3, true);

        /* Check APID */
        if(apid < 0 && apid > CCSDS_NUM_APIDS)
        {
            throw LuaException("invalid apid: %04X", apid);
        }

        /* Create Statistics Table */
        lua_newtable(L);
        LuaEngine::setAttrInt(L, "apid",            lua_obj->apidStats[apid].apid);
        LuaEngine::setAttrInt(L, "total (pkts)",    lua_obj->apidStats[apid].total_pkts);
        LuaEngine::setAttrInt(L, "total (bytes)",   lua_obj->apidStats[apid].total_bytes);
        LuaEngine::setAttrInt(L, "current (pkts)",  lua_obj->apidStats[apid].curr_pkts);
        LuaEngine::setAttrInt(L, "current (bytes)", lua_obj->apidStats[apid].curr_bytes);
        LuaEngine::setAttrInt(L, "dropped (pkts)",  lua_obj->apidStats[apid].pkts_dropped);
        LuaEngine::setAttrInt(L, "filtered (pkts)", lua_obj->apidStats[apid].pkts_filtered);
        LuaEngine::setAttrInt(L, "seq errors",      lua_obj->apidStats[apid].seq_errors);
        LuaEngine::setAttrInt(L, "seg errors",      lua_obj->apidStats[apid].seg_errors);
        LuaEngine::setAttrInt(L, "len errors",      lua_obj->apidStats[apid].len_errors);
        LuaEngine::setAttrInt(L, "odd errors",      lua_obj->apidStats[apid].odd_errors);
        LuaEngine::setAttrInt(L, "cks errors",      lua_obj->apidStats[apid].chksum_errors);
        LuaEngine::setAttrInt(L, "filter",          lua_obj->apidStats[apid].filter_factor);
        LuaEngine::setAttrInt(L, "max bps",         lua_obj->apidStats[apid].max_bps);
        LuaEngine::setAttrInt(L, "min bps",         lua_obj->apidStats[apid].min_bps);
        LuaEngine::setAttrInt(L, "avg bps",         lua_obj->apidStats[apid].avg_bps);
        LuaEngine::setAttrInt(L, "pass invalid",    lua_obj->passInvalid);
        LuaEngine::setAttrInt(L, "reset invalid",   lua_obj->resetInvalid);
        LuaEngine::setAttrInt(L, "pkt len",         lua_obj->pkt->getLEN());
        LuaEngine::setAttrInt(L, "pkt index",       lua_obj->pkt->getIndex());

        /* Log Stats */
        mlog(lvl, "apid:            %04X\n",   lua_obj->apidStats[apid].apid);
        mlog(lvl, "total (pkts):    %u\n",     lua_obj->apidStats[apid].total_pkts);
        mlog(lvl, "total (bytes):   %u\n",     lua_obj->apidStats[apid].total_bytes);
        mlog(lvl, "current (pkts):  %u\n",     lua_obj->apidStats[apid].curr_pkts);
        mlog(lvl, "current (bytes): %u\n",     lua_obj->apidStats[apid].curr_bytes);
        mlog(lvl, "dropped (pkts):  %u\n",     lua_obj->apidStats[apid].pkts_dropped);
        mlog(lvl, "filtered (pkts): %u\n",     lua_obj->apidStats[apid].pkts_filtered);
        mlog(lvl, "seq errors:      %u\n",     lua_obj->apidStats[apid].seq_errors);
        mlog(lvl, "seg errors:      %u\n",     lua_obj->apidStats[apid].seg_errors);
        mlog(lvl, "len errors:      %u\n",     lua_obj->apidStats[apid].len_errors);
        mlog(lvl, "odd errors:      %u\n",     lua_obj->apidStats[apid].odd_errors);
        mlog(lvl, "cks errors:      %u\n",     lua_obj->apidStats[apid].chksum_errors);
        mlog(lvl, "filter:          %u\n",     lua_obj->apidStats[apid].filter_factor);
        mlog(lvl, "max bps:         %lf\n",    lua_obj->apidStats[apid].max_bps);
        mlog(lvl, "min bps:         %lf\n",    lua_obj->apidStats[apid].min_bps);
        mlog(lvl, "avg bps:         %lf\n",    lua_obj->apidStats[apid].avg_bps);
        mlog(lvl, "pass invalid:    %d\n",     lua_obj->passInvalid);
        mlog(lvl, "reset invalid:   %d\n",     lua_obj->resetInvalid);
        mlog(lvl, "pkt len:         %d\n",     lua_obj->pkt->getLEN());
        mlog(lvl, "pkt index:       %d\n",     lua_obj->pkt->getIndex());

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting pass invalid state: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status, num_obj_to_return);
}

/*----------------------------------------------------------------------------
 * luaFilterPkt - :filter(<enable>, <start apid>, [<stop apid>])
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaFilterPkt (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketParser* lua_obj = (CcsdsPacketParser*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool enable = getLuaBoolean(L, 2);
        int start_apid = getLuaInteger(L, 3);
        int stop_apid = getLuaInteger(L, 4, true, start_apid);

        /* Set Filter */
        if(start_apid == ALL_APIDS)
        {
            for(int apid = 0; apid < CCSDS_NUM_APIDS; apid++)
            {
                lua_obj->filter[apid] = enable;
            }
        }
        else
        {
            for(int apid = start_apid; apid <= stop_apid; apid++)
            {
                if(apid >= 0 && apid < CCSDS_NUM_APIDS)
                {
                    lua_obj->filter[apid] = enable;
                }
            }
        }

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting filter: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaClearApidStats - :clear(<apid>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaClearApidStats (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketParser* lua_obj = (CcsdsPacketParser*)getLuaSelf(L, 1);

        /* Get Parameters */
        int apid = getLuaInteger(L, 2);

        /* Clear Stats */
        if(apid >= 0 && apid < CCSDS_NUM_APIDS)
        {
            LocalLib::set(&lua_obj->apidStats[apid], 0, sizeof(pktStats_t));
            lua_obj->apidStats[apid].apid = apid;
            lua_obj->apidStats[apid].min_bps = DBL_MAX;
        }
        else if(apid == ALL_APIDS)
        {
            for(int i = 0; i <= CCSDS_NUM_APIDS; i++)
            {
                LocalLib::set(&lua_obj->apidStats[i], 0, sizeof(pktStats_t));
                lua_obj->apidStats[i].apid = i;
                lua_obj->apidStats[i].min_bps = DBL_MAX;
            }
        }

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting filter: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaStripHdrOnPost: - striphdr(<enable>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketParser::luaStripHdrOnPost (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketParser* lua_obj = (CcsdsPacketParser*)getLuaSelf(L, 1);

        /* Get Parameters */
        bool strip_hdr = getLuaBoolean(L, 2);

        /* Set Strip Header on Post */
        lua_obj->stripHdrOnPost = strip_hdr;

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting strip header on post: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * processMsg
 *----------------------------------------------------------------------------*/
bool CcsdsPacketParser::processMsg (unsigned char* msg, int bytes)
{
    unsigned char*  recv_buffer = msg;
    int             recv_bytes = bytes;

    /* Count Total */
    parserBytes += bytes;

    /* Parse Bytes */
    int recv_index = 0;
    while(recv_index < recv_bytes)
    {
        /* Determine Number of Bytes left */
        int bytes_left = recv_bytes - recv_index;

        /* Parse Buffer */
        int parse_bytes = parser->parseBuffer(&recv_buffer[recv_index], bytes_left, pkt);
        if(parse_bytes >= 0)
        {
            if(!parserInSync) mlog(CRITICAL, "Parser %s re-established sync at %ld\n", getName(), parserBytes);
            parserInSync = true;

            /* Buffer Successfully Parsed */
            recv_index += parse_bytes;
        }
        else
        {
            if(parserInSync) mlog(CRITICAL, "Parser %s out of sync (%d) at %ld\n", getName(), parse_bytes, parserBytes);
            parserInSync = false;

            /* Attempt to Handle Error */
            pkt->resetPkt();
            parser->gotoInitState(true);
            recv_index++;
        }

        /* Full Packet Received */
        if(pkt->isFull())
        {
            uint16_t  apid    = pkt->getAPID();
            uint16_t  len     = pkt->getLEN();
            if(filter[apid])
            {
                /* Validate Packet */
                bool valid = true;
                if(pkt->getType() == CcsdsPacket::SPACE_PACKET)
                {
                    valid = isValid(pkt->getBuffer(), pkt->getLEN(), true);
                }
                else if(pkt->getType() == CcsdsPacket::ENCAPSULATION_PACKET)
                {
                    valid = pkt->getAPID() != CCSDS_ENCAP_PROTO_IDLE;
                }

                /* Process Packet */
                if(valid == true || passInvalid == true)
                {
                    /* Increment Stats */
                    apidStats[apid].total_pkts++;
                    apidStats[apid].curr_pkts++;
                    apidStats[ALL_APIDS].total_pkts++;
                    apidStats[ALL_APIDS].curr_pkts++;
                    apidStats[apid].total_bytes += len;
                    apidStats[apid].curr_bytes += len;
                    apidStats[ALL_APIDS].total_bytes += len;
                    apidStats[ALL_APIDS].curr_bytes += len;

                    /* Post Packet */
                    if(outQ != NULL)
                    {
                        int status = MsgQ::STATE_TIMEOUT;
                        while(isActive() && status == MsgQ::STATE_TIMEOUT)
                        {
                            /* Get Buffer and Buffer Size */
                            unsigned char* bufptr = pkt->getBuffer();
                            int buflen = pkt->getLEN();
                            if(stripHdrOnPost)
                            {
                                bufptr += pkt->getHdrSize();
                                buflen -= pkt->getHdrSize();
                            }

                            /* Check Buffer Size */
                            if(buflen <= 0)
                            {
                                mlog(CRITICAL, "Packet %04X has invalid size %d\n", pkt->getAPID(), buflen);
                                break;
                            }

                            /* Post Buffer */
                            status = outQ->postCopy(bufptr, buflen, SYS_TIMEOUT);
                            if((status != MsgQ::STATE_TIMEOUT) && (status < 0))
                            {
                                mlog(CRITICAL, "Packet %04X unable to be posted[%d] to output stream %s\n", pkt->getAPID(), status, outQ->getName());
                                apidStats[apid].pkts_dropped++;
                                apidStats[ALL_APIDS].pkts_dropped++;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    mlog(WARNING, "Packet %04X dropped\n", pkt->getAPID());
                    apidStats[apid].pkts_dropped++;
                    apidStats[ALL_APIDS].pkts_dropped++;
                }

                /* Handle Bad Packets */
                if(valid == false && resetInvalid == true)
                {
                    parser->gotoInitState(true);
                }
            }
            else
            {
                apidStats[apid].pkts_filtered++;
                apidStats[ALL_APIDS].pkts_filtered++;
            }

            /* Reset Packet */
            pkt->resetPkt();
        }
    }

    return true;
}

/*----------------------------------------------------------------------------
 * telemetry_thread
 *----------------------------------------------------------------------------*/
void* CcsdsPacketParser::telemetry_thread (void* parm)
{
    double now = 0.0, last = 0.0, elapsed = 0.0;

    CcsdsPacketParser* parser = (CcsdsPacketParser*)parm;

    while(parser->telemetryActive)
    {
        LocalLib::sleep(parser->telemetryWaitSeconds);

        /* Calculate Elapsed Time */
        last = now;
        now = TimeLib::latchtime();
        if(last == 0.0) continue; // skip first cycle
        elapsed = now - last;

        /* Post Packet Statistics */
        for(int apid = 0; apid <= CCSDS_NUM_APIDS; apid++)
        {
            if(parser->apidStats[apid].curr_pkts > 0)
            {
                /* Calculate Bits per Second */
                double bps = 0.0;
                if(elapsed > 0.0)
                {
                    bps = (parser->apidStats[apid].curr_bytes * 8) / elapsed;
                }

                /* Calculate Max Bps */
                if(bps > parser->apidStats[apid].max_bps)
                {
                    parser->apidStats[apid].max_bps = bps;
                }

                /* Calculate Min Bps */
                if(bps < parser->apidStats[apid].min_bps)
                {
                    parser->apidStats[apid].min_bps = bps;
                }

                /* Calculate Avg Bps */
                parser->apidStats[apid].avg_bps = integrateAverage(parser->apidStats[apid].bps_index++, parser->apidStats[apid].avg_bps, bps);

                /* Post Stats */
                if(parser->statQ != NULL)
                {
                    int status = parser->statQ->postCopy(&parser->apidStats[apid], sizeof(pktStats_t));
                    if(status <= 0)
                    {
                        mlog(CRITICAL, "(%d): failed to post apid stats to queue\n", status);
                    }
                }

                /* Clear and Return Current Stats */
                parser->apidStats[apid].curr_pkts = 0;
                parser->apidStats[apid].curr_bytes = 0;
            }
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * isValid
 *----------------------------------------------------------------------------*/
bool CcsdsPacketParser::isValid (unsigned char* _pkt, unsigned int _len, bool ignore_length)
{
    bool status = true;
    try
    {
        CcsdsSpacePacket candidate_packet(_pkt, _len);

        /* Get Primary Header Fields */
        unsigned int apid = candidate_packet.getAPID();
        unsigned int seq = candidate_packet.getSEQ();
        unsigned int len = candidate_packet.getLEN();
        seg_t        seg = candidate_packet.getSEQFLG();

        /* Command Packet Processing */
        if(candidate_packet.isCMD() && candidate_packet.hasSHDR())
        {
            if(candidate_packet.validChecksum() == false)
            {
                status = false;
                apidStats[apid].chksum_errors++;
                apidStats[ALL_APIDS].chksum_errors++;
                mlog(ERROR, "incorrect checksum in command packet 0x%04X: EXPECTED 0x%02X, CHECKSUM 0x%02X\n", apid, candidate_packet.computeChecksum(), candidate_packet.getChecksum());
            }
        }

        /* Telemetry Packet Processing */
        if(candidate_packet.isTLM())
        {
            /* Length Validation */
            if(!ignore_length)
            {
                if(len != _len)
                {
                    status = false;
                    apidStats[apid].len_errors++;
                    apidStats[ALL_APIDS].len_errors++;
                    mlog(ERROR, "pkt %04X failed strict length validation, exp: %d, act: %d\n", apid, _len, len);
                }
            }

            /* Sequence Validation */
            if(apidStats[apid].total_pkts > 2)
            {
                if(((apidStats[apid].last_seq + apidStats[apid].filter_factor) & 0x3FFF) != seq)
                {
                    status = false;
                    apidStats[apid].seq_errors++;
                    apidStats[ALL_APIDS].seq_errors++;
                    mlog(WARNING, "%s pkt %04X seq %04X unexpected from previous seq %04X (filter factor of %d)\n", getName(), apid, seq, apidStats[apid].last_seq, apidStats[apid].filter_factor);
                }
            }

            /* Increment Stats */
            if(seq >= apidStats[apid].last_seq)
            {
                apidStats[apid].filter_factor = seq - apidStats[apid].last_seq;
            }
            else
            {
                apidStats[apid].filter_factor = (0x4000 - apidStats[apid].last_seq) + seq;
            }
            apidStats[apid].last_seq = seq;
        }

        /* Segmentation Sequence Validation */
        if((apidStats[apid].last_seg != CcsdsSpacePacket::SEG_STOP) && (seg == CcsdsSpacePacket::SEG_START))
        {
            mlog(ERROR, "missing \"stop\" segmentation flags for APID %04X SEQ %04X (%02X %02X)\n", apid, seq, apidStats[apid].last_seg, seg);
            apidStats[apid].seg_errors++;
            apidStats[ALL_APIDS].seg_errors++;
            status = false;
        }
        else if((apidStats[apid].last_seg == CcsdsSpacePacket::SEG_STOP) && (seg != CcsdsSpacePacket::SEG_START))
        {
            mlog(ERROR, "missing \"start\" segmentation flags for APID %04X SEQ %04X (%02X %02X)\n", apid, seq, apidStats[apid].last_seg, seg);
            apidStats[apid].seg_errors++;
            apidStats[ALL_APIDS].seg_errors++;
            status = false;
        }
        apidStats[apid].last_seg = seg;
    }
    catch (std::invalid_argument& e)
    {
        mlog(ERROR, "Unable to create CCSDS packet in order to validate: %s\n", e.what());
        status = false;
    }

    return status;
}

/*----------------------------------------------------------------------------
 * str2pkttype
 *----------------------------------------------------------------------------*/
CcsdsPacket::type_t CcsdsPacketParser::str2pkttype(const char* str)
{
    if(str == NULL) return CcsdsPacket::INVALID_PACKET;
    else if(StringLib::match(str, "SPACE")) return CcsdsPacket::SPACE_PACKET;
    else if(StringLib::match(str, "ENCAP")) return CcsdsPacket::ENCAPSULATION_PACKET;
    return CcsdsPacket::INVALID_PACKET;
}

/*----------------------------------------------------------------------------
 * integrateAverage
 *----------------------------------------------------------------------------*/
double CcsdsPacketParser::integrateAverage(uint32_t statcnt, double curr_avg, double new_val)
{
    return ((curr_avg * (double)statcnt) + new_val) / ((double)statcnt + 1.0);
}