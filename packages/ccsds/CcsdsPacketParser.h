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

        bool            processMsg              (unsigned char* msg, int bytes);
        bool            isValid                 (unsigned char* _pkt, unsigned int _len, bool ignore_length);

        static CcsdsPacket::type_t  str2pkttype         (const char* str);
        static double               integrateAverage    (uint32_t statcnt, double curr_avg, double new_val);
};

#endif  /* __ccsds_packet_parser__ */
