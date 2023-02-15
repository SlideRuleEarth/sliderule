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

#include "CcsdsParserAOSFrameModule.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsParserAOSFrameModule::LuaMetaName = "CcsdsParserAOSFrameModule";
const struct luaL_Reg CcsdsParserAOSFrameModule::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - create(<Object Name>, <Spacecraft ID>, <Virtual Channel>, <Leading Strip Size>, <Sync Marker>, <Sync Offset>, <Fixed Frame Size>, <Leading Size>, <Trailing Size>)
 *
 *      Leading Strip Size - the number bytes to ignore in the beginning of the stream; this includes any synchronization marks
 *      Sync Marker - string of hex characters representing the sync marker, for example: 003201F3
 *      Sync Offset - number of bytes offset into the leading strip size that the sync marker starts; note that sync marker size plus offset has to be equal to or less than leading strip size
 *      Fixed Frame Size - the size of the AOS frame; includes the AOS header, the MPDU, and the trailer
 *      Leading Size - the size of the AOS frame header (not including the 2 bytes for the MPDU, these are automatically added to this value)
 *      Trailing Size - the size of the AOS frame trailer
 *
 *----------------------------------------------------------------------------*/
int CcsdsParserAOSFrameModule::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        long        scid        = getLuaInteger(L, 1);
        long        vcid        = getLuaInteger(L, 2);
        long        strip       = getLuaInteger(L, 3);
        const char* sync_str    = getLuaString(L, 4);
        long        offset      = getLuaInteger(L, 5);
        long        fixed       = getLuaInteger(L, 6);
        long        header      = getLuaInteger(L, 7);
        long        trailer     = getLuaInteger(L, 8);

        int sync_size = 0;
        uint8_t sync_marker[MAX_STR_SIZE];
        if(!StringLib::match(sync_str, "NOSYNC"))
        {
            sync_size = (int)StringLib::size(sync_str);
            if(sync_size <= 0 || ((sync_size / 2) + offset) > strip || sync_size % 2 != 0)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "sync marker is an invalid length: %d", sync_size);
            }
            else if(sync_size > MAX_STR_SIZE)
            {
                throw RunTimeException(CRITICAL, RTE_ERROR, "sync marker is too long: %d", sync_size);
            }
            sync_size /= 2;

            char numstr[5] = {'0', 'x', '\0', '\0', '\0'};
            for(int i = 0; i < (sync_size * 2); i+=2)
            {
                numstr[2] = sync_str[i];
                numstr[3] = sync_str[i+1];

                unsigned long val;
                if(!StringLib::str2ulong(numstr, &val))
                {
                    throw RunTimeException(CRITICAL, RTE_ERROR, "unable to parse sync marker at %d: %s", i, numstr);
                }

                sync_marker[i / 2] = (uint8_t)val;
            }
        }

        /* Return Dispatch Object */
        return createLuaObject(L, new CcsdsParserAOSFrameModule(L, scid, vcid, strip, sync_marker, sync_size, offset, fixed, header, trailer));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * parseBuffer
 *----------------------------------------------------------------------------*/
int CcsdsParserAOSFrameModule::parseBuffer (unsigned char* buffer, int bytes, CcsdsPacket* pkt)
{
    unsigned char*  parse_buffer = buffer;
    int             parse_bytes = bytes;
    int             parse_index = 0;

    /* Parse Buffer */
    while(parse_index < parse_bytes)
    {
        /* Determine Number of Bytes left */
        int bytes_left = parse_bytes - parse_index;

        if(state == LSTRIP)
        {
            /* Strip off leading bytes */
            if(lStripBytes <= bytes_left)
            {
                parse_index += lStripBytes;
                lStripBytes  = LStripSize;

                if(SyncMarkerSize > 0)  state = SYNC;
                else if(TStripSize > 0) state = TSTRIP;
                else                    state = HEADER;
            }
            else
            {
                lStripBytes -= bytes_left;
                parse_index += bytes_left;
            }
        }
        else if(state == SYNC)
        {
            while(state == SYNC && parse_index < parse_bytes && syncIndex < SyncMarkerSize)
            {
                if(parse_buffer[parse_index] != SyncMarker[syncIndex])
                {
                    syncIndex = 0;
                    if(inSync)
                    {
                        mlog(ERROR, "Lost sync in processing AOS frames in %s", getName());
                        inSync = false;
                        mpduOffsetSet = false;
                    }
                }
                else
                {
                    syncIndex++;
                    if(syncIndex == SyncMarkerSize)
                    {
                        syncIndex = 0;
                        if(!inSync)
                        {
                            mlog(INFO, "Synchronization of AOS frames acquired in %s", getName());
                            inSync = true;
                        }

                        if(TStripSize > 0) state = TSTRIP;
                        else               state = HEADER;
                    }
                }

                parse_index++;
            }
        }
        else if(state == TSTRIP)
        {
            /* Strip off trailing bytes */
            if(tStripBytes <= bytes_left)
            {
                parse_index += tStripBytes;
                tStripBytes  = TStripSize;
                state        = HEADER;
            }
            else
            {
                tStripBytes -= bytes_left;
                parse_index += bytes_left;
            }
        }
        else if(state == HEADER)
        {
            /* Copy into Primary Header Buffer */
            int cpylen = MIN(headerBytes, bytes_left);
            memcpy(&aosPrimaryHdr[FrameHeaderSize - headerBytes], &parse_buffer[parse_index], cpylen);
            headerBytes     -= cpylen;
            frameIndex      += cpylen;
            parse_index     += cpylen;

            /* Process and reset AOS primary header */
            if(headerBytes <= 0)
            {
                headerBytes = FrameHeaderSize;

                /* Calculate CRC of Header */
                frameCRC = crc16(aosPrimaryHdr, FrameHeaderSize, 0);

                /* Get Frame Counter and Virtual Channel */
                int curr_frame_counter = (((uint32_t)aosPrimaryHdr[2]) << 16) | (((uint32_t)aosPrimaryHdr[3]) << 8) | (((uint32_t)aosPrimaryHdr[4]));
                int curr_channel_id = aosPrimaryHdr[1] & 0x3F;

                /* Check Frame Counter */
                if(frameCounter != FRAME_COUNTER_UNSET)
                {
                    if(curr_frame_counter != ((frameCounter + 1) & 0xFFFFFF))
                    {
                        mlog(ERROR, "Frame counter in %s skipped at %d %d", getName(), frameCounter, curr_frame_counter);
                        mpduOffsetSet = false;
                        pkt->resetPkt();
                    }
                }

                /* Check Virtual Channel */
                if(curr_channel_id != VirtualChannel)
                {
                    mlog(ERROR, "Virtual channel in %s does not match, exp: %d, act: %d", getName(), VirtualChannel, curr_channel_id);
                }

                /* Set Frame Counter */
                frameCounter = curr_frame_counter;

                /* Set MPDU Offset First Time Only */
                if(!mpduOffsetSet)
                {
                    mpduOffset = (aosPrimaryHdr[FrameHeaderSize - 2] << 8) | aosPrimaryHdr[FrameHeaderSize - 1];
                    state = MPDU; //  find start of packet
                }
                else
                {
                    state = CCSDS;
                }
            }
        }
        else if(state == MPDU)
        {
            if(mpduOffset == FRAME_MPDU_CONTINUE || mpduOffset > bytes_left)
            {
                /* Check bytes left in current frame */
                if((bytes_left + frameIndex) > (FrameFixedSize - FrameTrailerSize))
                {
                    bytes_left = (FrameFixedSize - FrameTrailerSize) - frameIndex;
                }

                /* Update MPDU Offset */
                if(mpduOffset != FRAME_MPDU_CONTINUE)
                {
                    mpduOffset  -= bytes_left;
                }

                /* Go to end of frame (or as close as you can) */
                frameCRC        = crc16(&parse_buffer[parse_index], bytes_left, frameCRC);
                parse_index     += bytes_left;
                frameIndex      += bytes_left;
            }
            else // if(mpduOffset <= bytes_left)
            {
                /* Go to first packet in frame */
                frameCRC        = crc16(&parse_buffer[parse_index], mpduOffset, frameCRC);
                parse_index     += mpduOffset;
                frameIndex      += mpduOffset;
                state           = CCSDS;
                mpduOffsetSet   = true;
            }
        }
        else if(state == CCSDS)
        {
            /* Check that Bytes Left Fit in Frame */
            if((bytes_left + frameIndex) > (FrameFixedSize - FrameTrailerSize))
            {
                bytes_left = (FrameFixedSize - FrameTrailerSize) - frameIndex;
            }

            /* Call CCSDS Parser */
            int bytes_parsed = CcsdsParserModule::parseBuffer(&parse_buffer[parse_index], bytes_left, pkt);

            /* Check Status */
            if(bytes_parsed >= 0)
            {
                frameCRC = crc16(&parse_buffer[parse_index], bytes_parsed, frameCRC);
                frameIndex += bytes_parsed;
                parse_index += bytes_parsed;
            }
            else
            {
                return bytes_parsed; // error code
            }
        }
        else if(state == TRAILER)
        {
            int cpylen = MIN(trailerBytes, bytes_left);
            memcpy(&aosTrailer[FrameTrailerSize - trailerBytes], &parse_buffer[parse_index], cpylen);

            if(trailerBytes <= bytes_left)
            {
                /* Pull off AOS trailer */
                parse_index     += trailerBytes;
                trailerBytes     = FrameTrailerSize;
                frameIndex       = 0;

                if(LStripSize > 0)          state = LSTRIP;
                else if(SyncMarkerSize > 0) state = SYNC;
                else if(TStripSize > 0)     state = TSTRIP;
                else                        state = HEADER;

                /* Check CRC */
                if(FrameTrailerSize > 0)
                {
                    uint16_t trailer_crc = aosTrailer[0];
                    trailer_crc <<= 8;
                    trailer_crc |= aosTrailer[1];
                    if(trailer_crc != frameCRC)
                    {
                        mlog(ERROR, "Frame CRC in %s for frame %d does not match, exp: %04X, act: %04X", getName(), frameCounter, trailer_crc, frameCRC);
                        mpduOffsetSet = false;
                        pkt->resetPkt();
                    }
                }
            }
            else
            {
                trailerBytes    -= bytes_left;
                parse_index     += bytes_left;
            }
        }
        /* Error in State Machine - Bug in Code */
        else
        {
            assert(false);
        }

        /* Check for End of Frame */
        if(frameIndex >= (FrameFixedSize - FrameTrailerSize))
        {
            state = TRAILER;
        }

        /* Full Packet Received */
        if(pkt->isFull())
        {
            // Note that state is NOT reinitialized here
            // AOS frames stream CCSDS packets until fixed
            // size frame is full; there is no pad out
            break;
        }
    }

    return parse_index;
}

/*----------------------------------------------------------------------------
 * gotoInitState
 *----------------------------------------------------------------------------*/
void CcsdsParserAOSFrameModule::gotoInitState(bool reset)
{
    if(LStripSize > 0)          state = LSTRIP;
    else if(SyncMarkerSize > 0) state = SYNC;
    else if(TStripSize > 0)     state = TSTRIP;
    else                        state = HEADER;

    lStripBytes     = LStripSize;
    tStripBytes     = TStripSize;
    syncIndex       = 0;
    headerBytes     = FrameHeaderSize;
    trailerBytes    = FrameTrailerSize;
    frameIndex      = 0;

    memset(aosPrimaryHdr, 0, FrameHeaderSize);

    if(reset)
    {
        mpduOffsetSet = false;
        frameCounter = FRAME_COUNTER_UNSET;
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsParserAOSFrameModule::CcsdsParserAOSFrameModule(lua_State* L, int scid, int vcid, int strip_size, uint8_t* sync_marker, int sync_size, int sync_offset, int fixed_size, int header_size, int trailer_size):
    CcsdsParserModule(L, LuaMetaName, LuaMetaTable)
{
    SpacecraftId        = scid;
    VirtualChannel      = vcid;
    channelId           = (FRAME_VERSION_NUMBER << 14) | (SpacecraftId << 6) | VirtualChannel;

    LStripSize          = sync_offset;
    SyncMarkerSize      = sync_size;
    TStripSize          = strip_size - (sync_offset + sync_size);
    FrameFixedSize      = fixed_size;
    FrameHeaderSize     = header_size + 2; // for the state machine, the header needs to encompass the MPDU offset to first packet
    FrameTrailerSize    = trailer_size;

    SyncMarker = NULL;
    if(SyncMarkerSize > 0)
    {
        SyncMarker = new uint8_t [sync_size];
        memcpy(SyncMarker, sync_marker, sync_size);
    }

    aosPrimaryHdr = new unsigned char[FrameHeaderSize];
    if(FrameTrailerSize > 0)    aosTrailer = new uint8_t[FrameTrailerSize];
    else                        aosTrailer = NULL;

    inSync = true;

    gotoInitState(true);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsParserAOSFrameModule::~CcsdsParserAOSFrameModule(void)
{
    delete [] aosPrimaryHdr;
    if(aosTrailer) delete [] aosTrailer;
}

/*----------------------------------------------------------------------------
 * crc16
 *----------------------------------------------------------------------------*/
uint16_t CcsdsParserAOSFrameModule::crc16(uint8_t* data, uint32_t len, uint16_t crc)
{
    static const uint16_t CrcTable[256]=
    {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
    };

    uint32_t i;
    for (i = 0 ; i < len ; i++)
    {
       crc = ( (crc >> 8 ) & 0x00FF) ^ CrcTable[( crc ^ data[i]) & 0x00FF];
    }

    return crc;
}
