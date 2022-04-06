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

#include "CcsdsPacket.h"
#include "core.h"

#include <math.h>

/******************************************************************************
 * CCSDS SPACE PACKET - PUBLIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket::CcsdsSpacePacket(int len): CcsdsPacket(SPACE_PACKET)
{
    if(len < CCSDS_SPACE_HEADER_SIZE)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "buffer size must be greater than CCSDS header size");
    }
    else if(len < CCSDS_MAX_SPACE_PACKET_SIZE)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "buffer size cannot be greater than maximum CCSDS packet size");
    }
    else
    {
        buffer = new unsigned char[len];
        LocalLib::set(buffer, 0, len);
        index = 0; // allocate the space but start at beginning
        is_malloced = true;
        max_pkt_len = len;
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket::CcsdsSpacePacket(uint16_t apid, int len, bool clear): CcsdsPacket(SPACE_PACKET)
{
    if(len < CCSDS_SPACE_HEADER_SIZE)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "buffer must be present and of positive length");
    }
    else
    {
        buffer = new unsigned char[len];
        initPkt(apid, len, clear);
        index = CCSDS_SPACE_HEADER_SIZE; // start after primary header
        is_malloced = true;
        max_pkt_len = len;
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket::CcsdsSpacePacket(unsigned char* buf, int size, bool copy): CcsdsPacket(SPACE_PACKET)
{
    if(buf == NULL || size < CCSDS_SPACE_HEADER_SIZE)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "buffer must be present and of positive length");
    }
    else if(copy)
    {
        buffer = new unsigned char[size];
        LocalLib::copy(buffer, buf, size);
        max_pkt_len = size;
    }
    else
    {
        buffer = buf;
        max_pkt_len = size;
    }

    is_malloced = copy;
    index = size; // start at end of packet
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket::~CcsdsSpacePacket(void)
{
    if(is_malloced == true)
    {
        delete [] buffer;
    }
}

/*----------------------------------------------------------------------------
 * getAPID
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getAPID(void) const
{
    uint16_t sid = (buffer[0] << 8) + buffer[1];
    return sid & 0x07FF;
}

/*----------------------------------------------------------------------------
 * setAPID
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setAPID(int value)
{
    buffer[0] = (buffer[0] & 0xF8) | ((value >> 8) & 0x07);
    buffer[1] = value & 0xff;
}

/*----------------------------------------------------------------------------
 * hasSHDR
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::hasSHDR(void) const
{
    return (buffer[0] & 0x08) == 0x08;
}

/*----------------------------------------------------------------------------
 * setSHDR
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setSHDR(bool value)
{
    uint8_t flag = 0;
    if(value == true)
    {
        flag = 1;
    }

    buffer[0] = (buffer[0] & 0xf7) | ((flag << 3) & 0x08);
}

/*----------------------------------------------------------------------------
 * isCMD
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::isCMD(void) const
{
    return (buffer[0] & 0x10) == 0x10;
}

/*----------------------------------------------------------------------------
 * setCMD
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setCMD(void)
{
    uint8_t flag = 1;
    buffer[0] = (buffer[0] & 0xEF) | ((flag << 4) & 0x10);
}

/*----------------------------------------------------------------------------
 * isTLM
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::isTLM(void) const
{
    return (buffer[0] & 0x10) != 0x10;
}

/*----------------------------------------------------------------------------
 * setTLM
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setTLM(void)
{
    uint8_t flag = 0;
    buffer[0] = (buffer[0] & 0xEF) | ((flag << 4) & 0x10);
}

/*----------------------------------------------------------------------------
 * getVERS
 *
 *   Reads CCSDS version from primary header
 *----------------------------------------------------------------------------*/
uint8_t CcsdsSpacePacket::getVERS(void) const
{
    return (buffer[0] & 0xE0) >> 5;
}

/*----------------------------------------------------------------------------
 * setVERS
 *
 *   Write CCSDS version to primary header
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setVERS(uint8_t value)
{
    buffer[0] = (buffer[0] & 0x1F) | ((value << 5) & 0xE0);
}

/*----------------------------------------------------------------------------
 * getSEQ
 *
 *   Read sequence count from primary header
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getSEQ(void) const
{
    return ((buffer[2] & 0x3F) << 8) + buffer[3];
}

/*----------------------------------------------------------------------------
 * setSEQ
 *
 *   Write sequence count to primary header
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setSEQ(int value)
{
    buffer[2] = (buffer[2] & 0xC0) | ((value >> 8) & 0x3f);
    buffer[3] = value & 0xff;
}

/*----------------------------------------------------------------------------
 * getSEQFLG
 *
 *   Read sequence flags from primary header
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket::seg_flags_t CcsdsSpacePacket::getSEQFLG(void) const
{
    uint8_t flags = buffer[2] & 0xC0;

    switch(flags)
    {
        case SEG_START      :   return SEG_START;
        case SEG_CONTINUE   :   return SEG_CONTINUE;
        case SEG_STOP       :   return SEG_STOP;
        case SEG_NONE       :   return SEG_NONE;
        default             :   return SEG_ERROR;
    }
}

/*----------------------------------------------------------------------------
 * setSEQFLG
 *
 *   Write sequence flags to primary header
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setSEQFLG(seg_flags_t value)
{
    if(value != SEG_ERROR)
    {
        buffer[2] = (buffer[2] & 0x3F) | value;
    }
}

/*----------------------------------------------------------------------------
 * getLEN
 *
 *   Read total packet length from primary header
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getLEN(void) const
{
    int len = (buffer[4] << 8) + (buffer[5] + 7);
    if(max_pkt_len > 0)
    {
        if(len > max_pkt_len) mlog(WARNING, "out of bounds packet size detected: %d > %d", len, max_pkt_len);
        return MAX(MIN(len, max_pkt_len), 0);
    }
    else
    {
        return MAX(len, 0);
    }
}

/*----------------------------------------------------------------------------
 * setLEN
 *
 *   Write total packet length to primary header
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::setLEN(int value)
{
    buffer[4] = (value - 7) >> 8;
    buffer[5] = (value - 7) & 0xff;
}
/*----------------------------------------------------------------------------
 * getFunctionCode
 *
 *   Read function code from command secondary header
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getFunctionCode(void) const
{
    if(getLEN() > 6 && isCMD() && hasSHDR())
    {
        return buffer[CCSDS_FC_OFFSET] & 0x7f;
    }
    else
    {
        mlog(ERROR, "function code not present in packet %04X", getAPID());
        return CCSDS_ERROR;
    }
}

/*----------------------------------------------------------------------------
 * setFunctionCode
 *
 *   Write function code to command secondary header
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::setFunctionCode(uint8_t value)
{
    if(getLEN() > 6 && isCMD() && hasSHDR())
    {
        buffer[CCSDS_FC_OFFSET] = value & 0x7F;
        return true;
    }
    else
    {
        mlog(ERROR, "function code not present in packet %04X", getAPID());
        return false;
    }
}

/*----------------------------------------------------------------------------
 * getChecksum
 *
 *   Read checksum from command secondary header
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getChecksum(void) const
{
    if(getLEN() > 7 && isCMD() && hasSHDR())
    {
        return buffer[CCSDS_CS_OFFSET];
    }
    else
    {
        mlog(ERROR, "checksum not present in packet %04X", getAPID());
        return CCSDS_ERROR;
    }
}

/*----------------------------------------------------------------------------
 * setChecksum
 *
 *   Write checksum to command secondary header
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::setChecksum(uint8_t value)
{
    if(getLEN() > 7 && isCMD() && hasSHDR())
    {
        buffer[CCSDS_CS_OFFSET] = value;
        return true;
    }
    else
    {
        mlog(ERROR, "checksum not present in packet %04X", getAPID());
        return false;
    }
}

/*----------------------------------------------------------------------------
 * getCdsDays
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getCdsDays(void) const
{
    if(getLEN() > 7 && isTLM() && hasSHDR())
    {
        uint16_t days;

        days = buffer[6];
        days <<= 8;
        days |= buffer[7];

        return days;
    }
    else
    {
        mlog(ERROR, "timestamp not present in packet %04X", getAPID());
        return CCSDS_ERROR;
    }
}

/*----------------------------------------------------------------------------
 * setCdsDays
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::setCdsDays(uint16_t days)
{
    if(getLEN() > 7 && isTLM() && hasSHDR())
    {
        buffer[6] = (days >> 8) & 0xFF;
        buffer[7] = days & 0xFF;
        return true;
    }
    else
    {
        mlog(ERROR, "timestamp not present in packet %04X", getAPID());
        return false;
    }
}

/*----------------------------------------------------------------------------
 * getCdsMsecs
 *----------------------------------------------------------------------------*/
long CcsdsSpacePacket::getCdsMsecs(void) const
{
    if(getLEN() > 11 && isTLM() && hasSHDR())
    {
        uint32_t msecs;

        msecs = buffer[8];
        msecs <<= 8;
        msecs |= buffer[9];
        msecs <<= 8;
        msecs |= buffer[10];
        msecs <<= 8;
        msecs |= buffer[11];

        return msecs;
    }
    else
    {
        mlog(ERROR, "timestamp not present in packet %04X", getAPID());
        return CCSDS_ERROR;
    }
}

/*----------------------------------------------------------------------------
 * setCdsMsecs
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::setCdsMsecs(uint32_t msecs)
{
    if(getLEN() > 11 && isTLM() && hasSHDR())
    {
        buffer[8]  = (msecs >> 24) & 0xFF;
        buffer[9]  = (msecs >> 16) & 0xFF;
        buffer[10] = (msecs >> 8) & 0xFF;
        buffer[11] = msecs & 0xFF;
        return true;
    }
    else
    {
        mlog(ERROR, "timestamp not present in packet %04X", getAPID());
        return false;
    }
}

/*----------------------------------------------------------------------------
 * getCdsTime
 *----------------------------------------------------------------------------*/
double CcsdsSpacePacket::getCdsTime(void) const
{
    double days = getCdsDays();
    double ms = getCdsMsecs();

    return (days * (double)TIME_SECS_IN_A_DAY) + (ms / (double)TIME_MILLISECS_IN_A_SECOND);
}

/*----------------------------------------------------------------------------
 * getCdsTimeAsGmt
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket::pkt_time_t CcsdsSpacePacket::getCdsTimeAsGmt(void) const
{
    return TimeLib::cds2gmttime(getCdsDays(), getCdsMsecs());
}

/*----------------------------------------------------------------------------
 * setCdsTime
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::setCdsTime(double gps)
{
    uint32_t seconds          = (uint32_t)gps;
    uint32_t subseconds       = (uint32_t)((TIME_32BIT_FLOAT_MAX_VALUE) * (gps - (double)seconds));
    uint16_t days             = (uint16_t)(seconds / TIME_SECS_IN_A_DAY);
    uint32_t leftoverseconds  = seconds % TIME_SECS_IN_A_DAY;
    uint32_t milliseconds     = (TIME_MILLISECS_IN_A_SECOND * leftoverseconds) + (uint32_t) ( (double)(subseconds) / pow(2,32) * 1.0E+3 );;

    if(setCdsDays(days))
    {
        if(setCdsMsecs(milliseconds))
        {
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * initPkt
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::initPkt(int apid, int len, bool clear)
{
    if(clear)
    {
        LocalLib::set(buffer, 0, len);
    }
    else
    {
        LocalLib::set(buffer, 0, CCSDS_SPACE_HEADER_SIZE);    /* clear primary header only */
    }

    index = 0; // restart at beginning of packet

    setAPID(apid);
    setLEN(len);
}

/*----------------------------------------------------------------------------
 * resetPkt
 *----------------------------------------------------------------------------*/
void CcsdsSpacePacket::resetPkt(void)
{
    LocalLib::set(buffer, 0, CCSDS_SPACE_HEADER_SIZE);
    index = 0; // restart at beginning of packet
}

/*----------------------------------------------------------------------------
 * loadChecksum
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::loadChecksum(void)
{
    if(setChecksum(0) == true)
    {
        int cs = computeChecksum();
        if(cs != CCSDS_ERROR)
        {
            setChecksum(cs);
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * validChecksum
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::validChecksum(void) const
{
    int exp_cs = getChecksum();
    if(exp_cs != CCSDS_ERROR)
    {
        int act_cs = computeChecksum();
        if(exp_cs == act_cs)
        {
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * computeChecksum
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::computeChecksum(void) const
{
    uint16_t    len = getLEN();
    uint8_t     cs  = 0xFF;
    int         i   = 0;

    if(getLEN() > 7 && isCMD() && hasSHDR())
    {
        if((max_pkt_len <= 0) || (len <= max_pkt_len))
        {
            while (len--)
            {
                if(i != 7)
                {
                    cs ^= buffer[i];
                }
                i++;
            }
            return cs;
        }
    }

    return CCSDS_ERROR;
}

/*----------------------------------------------------------------------------
 * setIndex
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::setIndex(int offset)
{
    if(offset < getLEN())
    {
        index = offset;
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getIndex
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getIndex(void) const
{
    return index;
}

/*----------------------------------------------------------------------------
 * appendStream
 *
 *   returns number of bytes copied
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::appendStream(unsigned char* bytes, int len)
{
    int hdr_bytes_copied = 0;
    int pay_bytes_copied = 0;

    if(index < CCSDS_SPACE_HEADER_SIZE)
    {
        int hdr_left = CCSDS_SPACE_HEADER_SIZE - index;
        hdr_bytes_copied = len < hdr_left ? len : hdr_left;
        if(hdr_bytes_copied >= 0)
        {
            LocalLib::copy(&buffer[index], bytes, hdr_bytes_copied);
            index += hdr_bytes_copied;
        }
        else
        {
            return CCSDS_LEN_ERROR;
        }
    }

    if(index >= CCSDS_SPACE_HEADER_SIZE)
    {
        int payload_left = getLEN() - index;
        int stream_left = len - hdr_bytes_copied;

        pay_bytes_copied = payload_left < stream_left ? payload_left : stream_left;
        if( (max_pkt_len <= 0) ||
            (((pay_bytes_copied + index) <= max_pkt_len) &&
            ((pay_bytes_copied + hdr_bytes_copied) <= len) &&
            (pay_bytes_copied >= 0)) )
        {
            LocalLib::copy(&buffer[index], &bytes[hdr_bytes_copied], pay_bytes_copied);
            index += pay_bytes_copied;
        }
        else
        {
            mlog(CRITICAL, "Packet too large! %d", stream_left + hdr_bytes_copied);
            return CCSDS_LEN_ERROR;
        }
    }

    return hdr_bytes_copied + pay_bytes_copied;
}

/*----------------------------------------------------------------------------
 * isFull
 *----------------------------------------------------------------------------*/
bool CcsdsSpacePacket::isFull(void) const
{
    if(index >= CCSDS_SPACE_HEADER_SIZE)
    {
        if(index == getLEN())
        {
            return true;
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getBuffer
 *----------------------------------------------------------------------------*/
unsigned char* CcsdsSpacePacket::getBuffer(void)
{
    return buffer;
}

/*----------------------------------------------------------------------------
 * getPayload
 *----------------------------------------------------------------------------*/
unsigned char* CcsdsSpacePacket::getPayload(void)
{
    return &buffer[getHdrSize()];
}

/*----------------------------------------------------------------------------
 * getHdrSize
 *----------------------------------------------------------------------------*/
int CcsdsSpacePacket::getHdrSize(void) const
{
    if      (isTLM() && hasSHDR())  return CCSDS_TLMPAY_OFFSET;
    else if (isCMD() && hasSHDR())  return CCSDS_CMDPAY_OFFSET;
    else                            return CCSDS_SECHDR_OFFSET;
}

/*----------------------------------------------------------------------------
 * operator=
 *----------------------------------------------------------------------------*/
CcsdsSpacePacket& CcsdsSpacePacket::operator=(const CcsdsSpacePacket& rhp)
{
    /* Get Buffer and Buffer Length */
    if(max_pkt_len < rhp.max_pkt_len)
    {
        max_pkt_len = rhp.max_pkt_len;
        if(is_malloced) delete [] buffer;
        buffer = new unsigned char[max_pkt_len];
        is_malloced = true;
    }

    /* Copy Data Into Buffer and Reset Index */
    LocalLib::copy(buffer, rhp.buffer, rhp.getLEN());
    index = rhp.getLEN();

    /* Return This Object */
    return *this;
}

/*----------------------------------------------------------------------------
 * seg2str
 *----------------------------------------------------------------------------*/
const char* CcsdsSpacePacket::seg2str(seg_flags_t seg)
{
    switch(seg)
    {
        case SEG_START:     return "START";
        case SEG_CONTINUE:  return "CONTINUE";
        case SEG_STOP:      return "STOP";
        case SEG_NONE:      return "NONE";
        default:            return "ERROR";
    }
}

/******************************************************************************
 CCSDS ENCAPSULATION PACKET - PUBLIC FUNCTIONS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsEncapPacket::CcsdsEncapPacket(int len): CcsdsPacket(ENCAPSULATION_PACKET)
{
    if(len < CCSDS_ENCAP_HEADER_SIZE)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "buffer size must be greater than CCSDS header size");
    }
    else if(len < CCSDS_MAX_ENCAP_PACKET_SIZE)
    {
        throw RunTimeException(ERROR, RTE_ERROR, "buffer size cannot be greater than maximum CCSDS packet size");
    }
    else
    {
        buffer = new unsigned char[len];
        LocalLib::set(buffer, 0, len);
        index = 0; // allocate the space but start at beginning
        is_malloced = true;
        max_pkt_len = len;
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsEncapPacket::~CcsdsEncapPacket(void)
{
    if(is_malloced == true)
    {
        delete [] buffer;
    }
}

/*----------------------------------------------------------------------------
 * getAPID
 *
 *   Read application ID from protocol and protocol extension fields
 *----------------------------------------------------------------------------*/
int CcsdsEncapPacket::getAPID(void) const
{
    int proto1 = (buffer[0] & 0x1C) >> 2;
    int lol = buffer[0] & 0x03;

    if(proto1 == CCSDS_ENCAP_PROTO_PRIVATE)
    {
        if(lol > 0) return buffer[1]; // use both user defined and protocol extension fields
        else return proto1; // default to using the mission specific identifier
    }
    else if(proto1 != CCSDS_ENCAP_PROTO_EXTENSION)
    {
        return proto1;
    }
    else // use extension
    {
        if(lol > 1) return buffer[1] & 0x0F;
        else return 0; // this in an invalid case
    }
}

/*----------------------------------------------------------------------------
 * setAPID
 *
 *   Write application ID to protocol and protocol extension fields
 *----------------------------------------------------------------------------*/
void CcsdsEncapPacket::setAPID(int value)
{
    uint16_t hdr = (uint16_t)value;
    int lol = (hdr >> 10) & 0x03;
    buffer[0] = hdr >> 8;
    if(lol > 0) buffer[1] = hdr & 0xFF;
}

/*----------------------------------------------------------------------------
 * getSEQ
 *
 *   Read sequence count from CCSDS defined field
 *----------------------------------------------------------------------------*/
int CcsdsEncapPacket::getSEQ(void) const
{
    int lol = buffer[0] & 0x03;
    if(lol == 3)
    {
        uint16_t seq = buffer[2];
        seq <<= 8;
        seq |= buffer[3];
        return seq;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * setSEQ
 *
 *   Write sequence count to CCSDS defined field
 *----------------------------------------------------------------------------*/
void CcsdsEncapPacket::setSEQ(int value)
{
    int lol = buffer[0] & 0x03;
    if(lol == 3)
    {
        uint16_t seq = (uint16_t)value;
        buffer[2] = (seq >> 8) & 0xFF;
        buffer[3] = seq & 0xFF;
    }
}

/*----------------------------------------------------------------------------
 * getLEN
 *
 *   Read total packet length
 *----------------------------------------------------------------------------*/
int CcsdsEncapPacket::getLEN(void) const
{
    int pktlen = 0;

    int lol = buffer[0] & 0x03;
    if(lol == 0)
    {
        pktlen = 1;
    }
    else if(lol == 1)
    {
        pktlen = buffer[1];
    }
    else if(lol == 2)
    {
        uint16_t len = buffer[2];
        len <<= 8;
        len |= buffer[3];
        pktlen = len;
    }
    else if(lol == 3)
    {
        uint32_t len = buffer[4];
        len <<= 8;
        len |= buffer[5];
        len <<= 8;
        len |= buffer[6];
        len <<= 8;
        len |= buffer[7];
        pktlen = len;
    }

    return pktlen;
}

/*----------------------------------------------------------------------------
 * setLEN
 *
 *   Write total packet length
 *----------------------------------------------------------------------------*/
void CcsdsEncapPacket::setLEN(int value)
{
    int lol = buffer[0] & 0x03;
    if(lol == 1)
    {
        buffer[1] = (unsigned char)value;
    }
    else if(lol == 2)
    {
        uint16_t len = (uint16_t)value;
        buffer[2] = (len >> 8) & 0xFF;
        buffer[3] = len & 0xFF;
    }
    else if(lol == 3)
    {
        uint32_t len = (uint32_t)value;
        buffer[4] = (len >> 24) & 0xFF;
        buffer[5] = (len >> 16) & 0xFF;
        buffer[6] = (len >> 8) & 0xFF;
        buffer[7] = len & 0xFF;
    }
}

/*----------------------------------------------------------------------------
 * initPkt
 *----------------------------------------------------------------------------*/
void CcsdsEncapPacket::initPkt(int apid, int len, bool clear)
{
    if(clear) LocalLib::set(buffer, 0, len);
    else LocalLib::set(buffer, 0, CCSDS_ENCAP_HEADER_SIZE);    /* clear header only */

    index = 0; // restart at beginning of packet

    setAPID(apid);
    setLEN(len);
}

/*----------------------------------------------------------------------------
 * resetPkt
 *----------------------------------------------------------------------------*/
void CcsdsEncapPacket::resetPkt(void)
{
    LocalLib::set(buffer, 0, CCSDS_ENCAP_HEADER_SIZE);
    index = 0; // restart at beginning of packet
}

/*----------------------------------------------------------------------------
 * setIndex
 *----------------------------------------------------------------------------*/
bool CcsdsEncapPacket::setIndex(int offset)
{
    if(offset < getLEN())
    {
        index = offset;
        return true;
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getIndex
 *----------------------------------------------------------------------------*/
int CcsdsEncapPacket::getIndex(void) const
{
    return index;
}

/*----------------------------------------------------------------------------
 * appendStream
 *
 *   returns number of bytes copied
 *----------------------------------------------------------------------------*/
int CcsdsEncapPacket::appendStream(unsigned char* bytes, int len)
{
    int len_bytes_copied = 0;
    int hdr_bytes_copied = 0;
    int pay_bytes_copied = 0;
    int hdr_size = 0;

    if(index == 0 && len > 0)
    {
        len_bytes_copied = 1;
        buffer[index++] = bytes[0];
    }

    if(index > 0)
    {
        hdr_size = getHdrSize();
        if(index < hdr_size)
        {
            int hdr_left = hdr_size - index;
            int stream_left = len - len_bytes_copied;

            hdr_bytes_copied = stream_left < hdr_left ? stream_left : hdr_left;
            if(hdr_bytes_copied >= 0)
            {
                LocalLib::copy(&buffer[index], &bytes[len_bytes_copied], hdr_bytes_copied);
                index += hdr_bytes_copied;
            }
            else
            {
                return CCSDS_LEN_ERROR;
            }
        }
    }
    if(index >= hdr_size)
    {
        int payload_left = getLEN() - index;
        int stream_left = len - len_bytes_copied - hdr_bytes_copied;

        pay_bytes_copied = payload_left < stream_left ? payload_left : stream_left;
        if( (max_pkt_len <= 0) ||
            (((pay_bytes_copied + index) <= max_pkt_len) &&
            ((pay_bytes_copied + hdr_bytes_copied + len_bytes_copied) <= len) &&
            (pay_bytes_copied >= 0)) )
        {
            LocalLib::copy(&buffer[index], &bytes[hdr_bytes_copied + len_bytes_copied], pay_bytes_copied);
            index += pay_bytes_copied;
        }
        else
        {
            mlog(CRITICAL, "Packet size mismatch! %d", stream_left + hdr_bytes_copied + len_bytes_copied);
            return CCSDS_LEN_ERROR;
        }
    }

    return len_bytes_copied + hdr_bytes_copied + pay_bytes_copied;
}

/*----------------------------------------------------------------------------
 * isFull
 *----------------------------------------------------------------------------*/
bool CcsdsEncapPacket::isFull(void) const
{
    if(index >= CCSDS_ENCAP_HEADER_SIZE)
    {
        if(index >= getHdrSize())
        {
            if(index == getLEN())
            {
                return true;
            }
        }
    }

    return false;
}

/*----------------------------------------------------------------------------
 * getBuffer
 *----------------------------------------------------------------------------*/
unsigned char* CcsdsEncapPacket::getBuffer(void)
{
    return buffer;
}

/*----------------------------------------------------------------------------
 * getPayload
 *----------------------------------------------------------------------------*/
unsigned char* CcsdsEncapPacket::getPayload(void)
{
    return &buffer[getHdrSize()];
}

/*----------------------------------------------------------------------------
 * getHdrSize
 *----------------------------------------------------------------------------*/
int CcsdsEncapPacket::getHdrSize(void) const
{
    int lol = buffer[0] & 0x03;
         if(lol == 0) return 1;
    else if(lol == 1) return 2;
    else if(lol == 2) return 4;
    else              return 8; // lol == 3
}
