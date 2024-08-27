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

#include "CcsdsPacketParser.h"
#include "CcsdsParserModule.h"
#include "OsApi.h"

#include <float.h>


/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsPacketParser::LUA_META_NAME = "CcsdsPacketParser";
const struct luaL_Reg CcsdsPacketParser::LUA_META_TABLE[] = {
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
    CcsdsParserModule* _parser = NULL;
    try
    {
        /* Get Parameters */
        _parser                 = dynamic_cast<CcsdsParserModule*>(getLuaObject(L, 1, CcsdsParserModule::OBJECT_TYPE));
        const char* type_str    = getLuaString(L, 2);
        const char* inq_name    = getLuaString(L, 3);
        const char* outq_name   = getLuaString(L, 4, true, NULL);
        const char* statq_name  = getLuaString(L, 5, true, NULL);

        /* Get Packet Type */
        const CcsdsPacket::type_t pkt_type = str2pkttype(type_str);
        if(pkt_type == CcsdsPacket::INVALID_PACKET)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid packet type: %s", type_str);
        }

        /* Create Packet Parser */
        return createLuaObject(L, new CcsdsPacketParser(L, _parser, pkt_type, inq_name, outq_name, statq_name));
    }
    catch(const RunTimeException& e)
    {
        if(_parser) _parser->releaseLuaObject();
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
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
    MsgProcessor(L, inq_name, LUA_META_NAME, LUA_META_TABLE)
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
    memset(&apidStats, 0, sizeof(apidStats));
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
    delete outQ;
    delete statQ;
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
        CcsdsPacketParser* lua_obj = dynamic_cast<CcsdsPacketParser*>(getLuaSelf(L, 1));

        /* Get Parameters */
        const bool pass_invalid = getLuaBoolean(L, 2);

        /* Set Pass Invalid */
        lua_obj->passInvalid = pass_invalid;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting pass invalid state: %s", e.what());
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
        CcsdsPacketParser* lua_obj = dynamic_cast<CcsdsPacketParser*>(getLuaSelf(L, 1));

        /* Get Parameters */
        const bool reset_invalid = getLuaBoolean(L, 2);

        /* Set Reset Invalid */
        lua_obj->resetInvalid = reset_invalid;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting pass invalid state: %s", e.what());
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
        CcsdsPacketParser* lua_obj = dynamic_cast<CcsdsPacketParser*>(getLuaSelf(L, 1));

        /* Get Parameters */
        const long            apid    = getLuaInteger(L, 2);
        const event_level_t   lvl     = (event_level_t)getLuaInteger(L, 3, true);

        /* Check APID */
        if(apid < 0 && apid > CCSDS_NUM_APIDS)
        {
            throw RunTimeException(CRITICAL, RTE_ERROR, "invalid apid: %04X", (unsigned int)apid);
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
        mlog(lvl, "apid:            %04X",   lua_obj->apidStats[apid].apid);
        mlog(lvl, "total (pkts):    %u",     lua_obj->apidStats[apid].total_pkts);
        mlog(lvl, "total (bytes):   %u",     lua_obj->apidStats[apid].total_bytes);
        mlog(lvl, "current (pkts):  %u",     lua_obj->apidStats[apid].curr_pkts);
        mlog(lvl, "current (bytes): %u",     lua_obj->apidStats[apid].curr_bytes);
        mlog(lvl, "dropped (pkts):  %u",     lua_obj->apidStats[apid].pkts_dropped);
        mlog(lvl, "filtered (pkts): %u",     lua_obj->apidStats[apid].pkts_filtered);
        mlog(lvl, "seq errors:      %u",     lua_obj->apidStats[apid].seq_errors);
        mlog(lvl, "seg errors:      %u",     lua_obj->apidStats[apid].seg_errors);
        mlog(lvl, "len errors:      %u",     lua_obj->apidStats[apid].len_errors);
        mlog(lvl, "odd errors:      %u",     lua_obj->apidStats[apid].odd_errors);
        mlog(lvl, "cks errors:      %u",     lua_obj->apidStats[apid].chksum_errors);
        mlog(lvl, "filter:          %u",     lua_obj->apidStats[apid].filter_factor);
        mlog(lvl, "max bps:         %lf",    lua_obj->apidStats[apid].max_bps);
        mlog(lvl, "min bps:         %lf",    lua_obj->apidStats[apid].min_bps);
        mlog(lvl, "avg bps:         %lf",    lua_obj->apidStats[apid].avg_bps);
        mlog(lvl, "pass invalid:    %d",     lua_obj->passInvalid);
        mlog(lvl, "reset invalid:   %d",     lua_obj->resetInvalid);
        mlog(lvl, "pkt len:         %d",     lua_obj->pkt->getLEN());
        mlog(lvl, "pkt index:       %d",     lua_obj->pkt->getIndex());

        /* Set Success */
        status = true;
        num_obj_to_return = 2;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting pass invalid state: %s", e.what());
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
        CcsdsPacketParser* lua_obj = dynamic_cast<CcsdsPacketParser*>(getLuaSelf(L, 1));

        /* Get Parameters */
        const bool enable = getLuaBoolean(L, 2);
        const int start_apid = getLuaInteger(L, 3);
        const int stop_apid = getLuaInteger(L, 4, true, start_apid);

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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting filter: %s", e.what());
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
        CcsdsPacketParser* lua_obj = dynamic_cast<CcsdsPacketParser*>(getLuaSelf(L, 1));

        /* Get Parameters */
        const int apid = getLuaInteger(L, 2);

        /* Clear Stats */
        if(apid >= 0 && apid < CCSDS_NUM_APIDS)
        {
            memset(&lua_obj->apidStats[apid], 0, sizeof(pktStats_t));
            lua_obj->apidStats[apid].apid = apid;
            lua_obj->apidStats[apid].min_bps = DBL_MAX;
        }
        else if(apid == ALL_APIDS)
        {
            for(int i = 0; i <= CCSDS_NUM_APIDS; i++)
            {
                memset(&lua_obj->apidStats[i], 0, sizeof(pktStats_t));
                lua_obj->apidStats[i].apid = i;
                lua_obj->apidStats[i].min_bps = DBL_MAX;
            }
        }

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting filter: %s", e.what());
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
        CcsdsPacketParser* lua_obj = dynamic_cast<CcsdsPacketParser*>(getLuaSelf(L, 1));

        /* Get Parameters */
        const bool strip_hdr = getLuaBoolean(L, 2);

        /* Set Strip Header on Post */
        lua_obj->stripHdrOnPost = strip_hdr;

        /* Set Success */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error setting strip header on post: %s", e.what());
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * deinitProcessing
 *----------------------------------------------------------------------------*/
bool CcsdsPacketParser::deinitProcessing (void)
{
    const int status = outQ->postCopy("", 0, SYS_TIMEOUT);
    if(status < 0)
    {
        mlog(CRITICAL, "Failed (%d) to post terminator to %s", status, outQ->getName());
    }

    return status > 0;
}

/*----------------------------------------------------------------------------
 * processMsg
 *----------------------------------------------------------------------------*/
bool CcsdsPacketParser::processMsg (unsigned char* msg, int bytes)
{
    unsigned char*  recv_buffer = msg;
    const int       recv_bytes = bytes;

    /* Count Total */
    parserBytes += bytes;

    /* Parse Bytes */
    int recv_index = 0;
    while(recv_index < recv_bytes)
    {
        /* Determine Number of Bytes left */
        const int bytes_left = recv_bytes - recv_index;

        /* Parse Buffer */
        const int parse_bytes = parser->parseBuffer(&recv_buffer[recv_index], bytes_left, pkt);
        if(parse_bytes >= 0)
        {
            if(!parserInSync) mlog(INFO, "Parser %s re-established sync at %ld", getName(), parserBytes);
            parserInSync = true;

            /* Buffer Successfully Parsed */
            recv_index += parse_bytes;
        }
        else
        {
            if(parserInSync) mlog(INFO, "Parser %s out of sync (%d) at %ld", getName(), parse_bytes, parserBytes);
            parserInSync = false;

            /* Attempt to Handle Error */
            pkt->resetPkt();
            parser->gotoInitState(true);
            recv_index++;
        }

        /* Full Packet Received */
        if(pkt->isFull())
        {
            const uint16_t  apid    = pkt->getAPID();
            const uint16_t  len     = pkt->getLEN();
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
                                mlog(CRITICAL, "Packet %04X has invalid size %d", pkt->getAPID(), buflen);
                                break;
                            }

                            /* Post Buffer */
                            status = outQ->postCopy(bufptr, buflen, SYS_TIMEOUT);
                            if((status != MsgQ::STATE_TIMEOUT) && (status < 0))  // NOLINT(misc-redundant-expression)
                            {
                                mlog(CRITICAL, "Packet %04X unable to be posted[%d] to output stream %s", pkt->getAPID(), status, outQ->getName());
                                apidStats[apid].pkts_dropped++;
                                apidStats[ALL_APIDS].pkts_dropped++;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    mlog(WARNING, "Packet %04X dropped", pkt->getAPID());
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
    double now = 0.0;
    double elapsed = 0.0;

    CcsdsPacketParser* parser = static_cast<CcsdsPacketParser*>(parm);

    while(parser->telemetryActive)
    {
        OsApi::sleep(parser->telemetryWaitSeconds);

        /* Calculate Elapsed Time */
        const double last = now;
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
                    const int status = parser->statQ->postCopy(&parser->apidStats[apid], sizeof(pktStats_t));
                    if(status <= 0)
                    {
                        mlog(CRITICAL, "(%d): failed to post apid stats to queue", status);
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
        const CcsdsSpacePacket candidate_packet(_pkt, _len);

        /* Get Primary Header Fields */
        const unsigned int apid = candidate_packet.getAPID();
        const unsigned int seq = candidate_packet.getSEQ();
        const seg_t        seg = candidate_packet.getSEQFLG();

        /* Command Packet Processing */
        if(candidate_packet.isCMD() && candidate_packet.hasSHDR())
        {
            if(candidate_packet.validChecksum() == false)
            {
                status = false;
                apidStats[apid].chksum_errors++;
                apidStats[ALL_APIDS].chksum_errors++;
                mlog(ERROR, "incorrect checksum in command packet 0x%04X: EXPECTED 0x%02X, CHECKSUM 0x%02X", apid, candidate_packet.computeChecksum(), candidate_packet.getChecksum());
            }
        }

        /* Telemetry Packet Processing */
        if(candidate_packet.isTLM())
        {
            /* Length Validation */
            if(!ignore_length)
            {
                const unsigned int len = candidate_packet.getLEN();
                if(len != _len)
                {
                    status = false;
                    apidStats[apid].len_errors++;
                    apidStats[ALL_APIDS].len_errors++;
                    mlog(ERROR, "pkt %04X failed strict length validation, exp: %d, act: %d", apid, _len, len);
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
                    mlog(WARNING, "%s pkt %04X seq %04X unexpected from previous seq %04X (filter factor of %d)", getName(), apid, seq, apidStats[apid].last_seq, apidStats[apid].filter_factor);
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
            mlog(ERROR, "missing \"stop\" segmentation flags for APID %04X SEQ %04X (%02X %02X)", apid, seq, apidStats[apid].last_seg, seg);
            apidStats[apid].seg_errors++;
            apidStats[ALL_APIDS].seg_errors++;
            status = false;
        }
        else if((apidStats[apid].last_seg == CcsdsSpacePacket::SEG_STOP) && (seg != CcsdsSpacePacket::SEG_START))
        {
            mlog(ERROR, "missing \"start\" segmentation flags for APID %04X SEQ %04X (%02X %02X)", apid, seq, apidStats[apid].last_seg, seg);
            apidStats[apid].seg_errors++;
            apidStats[ALL_APIDS].seg_errors++;
            status = false;
        }
        apidStats[apid].last_seg = seg;
    }
    catch (RunTimeException& e)
    {
        mlog(e.level(), "Unable to create CCSDS packet in order to validate: %s", e.what());
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