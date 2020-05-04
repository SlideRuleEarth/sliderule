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
        mlog(ERROR, "Packet length exceeds maximum length in %s: %d > %d\n", getName(), pkt_len, maxLength);
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
            mlog(WARNING, "unable to load checksum into packetized record %04X:%02X\n", apid, functionCode);
        }
    }

    /* Post Packet */
    int status = outQ->postCopy(pkt.getBuffer(), pkt.getLEN());
    if(status <= 0)
    {
        mlog(ERROR, "failed to post packetized record %04X\n", apid);
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
    int leap_secs = (TimeLib::getleapmsec((int64_t)unix_secs * TIME_MILLISECS_IN_A_SECOND) / TIME_MILLISECS_IN_A_SECOND);

    /* Return GPS Seconds */
    return gps_secs - (double)leap_secs;
}
