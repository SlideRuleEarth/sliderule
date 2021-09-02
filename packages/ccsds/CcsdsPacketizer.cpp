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

#include "CcsdsPacketizer.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsPacketizer::LuaMetaName = "CcsdsPacketizer";
const struct luaL_Reg CcsdsPacketizer::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - parser(<parser>, <type - ccsds.ENCAP|ccsds.SPACE>, <inq>, <outq>, <statq>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketizer::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* inq         = getLuaString(L, 1);
        const char* outq        = getLuaString(L, 2);
        int         apid        = getLuaInteger(L, 3);
        int         pkttype     = getLuaInteger(L, 4);
        int         fc          = getLuaInteger(L, 5, true, 0);
        int         maxsize     = getLuaInteger(L, 6, true, CcsdsPacketizer::DEFAULT_MAX_PACKET_SIZE);

        /* Create Packet Parser */
        return createLuaObject(L, new CcsdsPacketizer(L, inq, outq, pkttype, (uint16_t)apid, fc, (uint16_t)maxsize));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsPacketizer::CcsdsPacketizer(lua_State* L, const char* inq_name, const char* outq_name, int _pkttype, uint16_t _apid, uint8_t _fc, uint16_t _len):
    MsgProcessor(L, inq_name, LuaMetaName, LuaMetaTable)
{
    pktType = _pkttype;
    apid = _apid;
    functionCode = _fc;
    maxLength = _len;
    if(pktType == TLM_PKT) hdrLength = CcsdsSpacePacket::CCSDS_TLMPAY_OFFSET;
    else if(pktType == CMD_PKT) hdrLength = CcsdsSpacePacket::CCSDS_CMDPAY_OFFSET;
    else hdrLength = 0;
    LocalLib::set(seqTable, 0, sizeof(seqTable));

    /* Set Streams - Required: names cannot be NULL */
    outQ = new Publisher(outq_name);

    /* Start Processor */
    start();
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsPacketizer::~CcsdsPacketizer(void)
{
    /* Stop Processor */
    stop();

    delete outQ;
}

/*----------------------------------------------------------------------------
 * processMsg
 *----------------------------------------------------------------------------*/
bool CcsdsPacketizer::processMsg (unsigned char* msg, int bytes)
{
    int pkt_len = hdrLength + bytes;

    if(pkt_len > maxLength)
    {
        mlog(ERROR, "Packet length exceeds maximum length in %s: %d > %d", getName(), pkt_len, maxLength);
        return false;
    }

    CcsdsSpacePacket pkt(apid, pkt_len, false);
    pkt.setSHDR(true);
    pkt.setIndex(hdrLength);
    if(pktType == TLM_PKT)
    {
        pkt.setTLM();
        pkt.setSEQFLG(CcsdsSpacePacket::SEG_NONE);
        pkt.setSEQ(seqTable[pkt.getAPID()]++);
        pkt.setCdsTime(getCurrGPSTime());
        pkt.appendStream(msg, bytes);
    }
    else
    {
        pkt.setCMD();
        pkt.setSEQFLG(CcsdsSpacePacket::SEG_NONE);
        pkt.setFunctionCode(functionCode);
        pkt.appendStream(msg, bytes);
        if(pkt.loadChecksum() == false)
        {
            mlog(WARNING, "unable to load checksum into packetized record %04X:%02X", apid, functionCode);
        }
    }

    /* Post Packet */
    int status = outQ->postCopy(pkt.getBuffer(), pkt.getLEN());
    if(status <= 0)
    {
        mlog(ERROR, "failed to post packetized record %04X", apid);
    }

    return true;
}

/*----------------------------------------------------------------------------
 * getCurrGPSTime
 *----------------------------------------------------------------------------*/
double CcsdsPacketizer::getCurrGPSTime(void)
{
    /* Get current unix/UTC time */
    double unix_secs = (double)time(NULL);

    /* There was an offset of 315964800 secs between Unix and GPS time when GPS time began */
    double gps_secs = unix_secs - 315964800;

    /* Get count of leap seconds between start of GPS epoch and now */
    int leap_secs = (TimeLib::getleapms((int64_t)unix_secs * TIME_MILLISECS_IN_A_SECOND) / TIME_MILLISECS_IN_A_SECOND);

    /* Return GPS Seconds */
    return gps_secs - (double)leap_secs;
}
