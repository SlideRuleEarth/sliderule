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
