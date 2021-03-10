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

#ifndef __ccsds_packet_parser__
#define __ccsds_packet_parser__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsPacket.h"
#include "CcsdsParserModule.h"
#include "MsgProcessor.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * CCSDS PACKET PARSER CLASS
 ******************************************************************************/

class CcsdsPacketParser: public MsgProcessor
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LuaMetaName;
        static const struct luaL_Reg LuaMetaTable[];

        static const unsigned int IGNORE_LENGTH             = 0;
        static const unsigned int MAX_ALLOWED_PKT_LENGTH    = 0xFFFF;
        static const unsigned int MIN_ALLOWED_PKT_LENGTH    = 12;

        /*--------------------------------------------------------------------
         * Types
         *--------------------------------------------------------------------*/

        typedef CcsdsSpacePacket::seg_flags_t seg_t;

        typedef struct {
            uint32_t    apid;
            uint32_t    total_pkts;
            uint32_t    total_bytes;
            uint32_t    curr_pkts;
            uint32_t    curr_bytes;
            uint32_t    pkts_dropped;
            uint32_t    pkts_filtered;
            uint32_t    seq_errors;
            uint32_t    seg_errors;
            uint32_t    len_errors;
            uint32_t    odd_errors;
            uint32_t    chksum_errors;
            uint32_t    filter_factor;
            uint32_t    last_seq;
            seg_t       last_seg;
            uint32_t    bps_index;
            double      max_bps;
            double      min_bps;
            double      avg_bps;
        } pktStats_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        CcsdsParserModule*  parser;
        CcsdsPacket::type_t pktType;
        CcsdsPacket*        pkt;

        int                 telemetryWaitSeconds;
        bool                passInvalid;
        bool                resetInvalid;
        bool                stripHdrOnPost;

        bool                telemetryActive;
        Thread*             telemetryThread;

        bool                filter[CCSDS_NUM_APIDS];
        pktStats_t          apidStats[CCSDS_NUM_APIDS + 1]; // last element is for all packets

        Publisher*          outQ;
        Publisher*          statQ;

        bool                parserInSync;
        long                parserBytes;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsPacketParser       (lua_State* L, CcsdsParserModule* _parser, CcsdsPacket::type_t _type, const char* inq_name, const char* outq_name, const char* statq_name);
                        ~CcsdsPacketParser      (void);

        static int      luaPassInvalid          (lua_State* L);
        static int      luaResetInvalid         (lua_State* L);
        static int      luaLogPktStats          (lua_State* L);
        static int      luaFilterPkt            (lua_State* L);
        static int      luaClearApidStats       (lua_State* L);
        static int      luaStripHdrOnPost       (lua_State* L);

        static void*    telemetry_thread        (void* parm);

        bool            deinitProcessing        (void) override;
        bool            processMsg              (unsigned char* msg, int bytes) override;
        bool            isValid                 (unsigned char* _pkt, unsigned int _len, bool ignore_length);

        static CcsdsPacket::type_t  str2pkttype         (const char* str);
        static double               integrateAverage    (uint32_t statcnt, double curr_avg, double new_val);
};

#endif  /* __ccsds_packet_parser__ */
