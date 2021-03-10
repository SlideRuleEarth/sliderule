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

#ifndef __ccsds_packetizer__
#define __ccsds_packetizer__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "MsgQ.h"
#include "CcsdsPacket.h"
#include "MsgProcessor.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PACKETIZER CLASS
 ******************************************************************************/

class CcsdsPacketizer: public MsgProcessor
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const uint16_t DEFAULT_MAX_PACKET_SIZE = 2048;
        static const int TLM_PKT = 0;
        static const int CMD_PKT = 1;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        static uint16_t     seqCnt[CCSDS_NUM_APIDS];

        Publisher*          outQ;

        int                 pktType; // TLM | CMD
        uint16_t            apid; // 11 bits
        uint8_t             functionCode; // for commands only
        uint16_t            maxLength;
        uint16_t            hdrLength;

        uint16_t            seqTable[CCSDS_NUM_APIDS];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsPacketizer     (lua_State* L, const char* inq_name, const char* outq_name, int _pkttype, uint16_t _apid, uint8_t _fc, uint16_t _len=DEFAULT_MAX_PACKET_SIZE);
                        ~CcsdsPacketizer    (void);

        bool            processMsg          (unsigned char* msg, int bytes); // OVERLOAD

        static double   getCurrGPSTime      (void);
};


#endif  /* __ccsds_packetizer__ */
