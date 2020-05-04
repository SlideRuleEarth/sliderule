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

#ifndef __cfs_interface__
#define __cfs_interface__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsPacket.h"
#include "CommandableObject.h"
#include "MsgQ.h"
#include "UdpSocket.h"
#include "StatisticRecord.h"
#include "OsApi.h"

/******************************************************************************
 * CFS INTERFACE CLASS
 ******************************************************************************/

class CfsInterface: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;

        static const int TLM_APIDS = CCSDS_NUM_APIDS;
        static const int CMD_APIDS = CCSDS_NUM_APIDS + 1;

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef CcsdsSpacePacket::seg_flags_t seg_t;

        typedef struct {
            uint32_t      apid;
            uint32_t      segs_read;
            uint32_t      segs_errors;
            uint32_t      segs_forwarded;
            uint32_t      segs_dropped;
            uint32_t      total_bytes;
            uint32_t      total_pkts;
            uint32_t      seq_errors;
            uint32_t      seg_errors;
            uint32_t      len_errors;
            uint32_t      chksum_errors;
            uint32_t      filter_factor;
            uint32_t      last_seq;
            seg_t       last_seg;
            int64_t       first_pkt_time; // time first packet arrived after clear
            int64_t       total_pkt_time; // total amount time packet since last clear
            double      avg_bps;
        } pktStats_t;

        class PktStats: public StatisticRecord<pktStats_t>
        {
            public:

                static const char* rec_type;
                static RecordObject::fieldDef_t rec_def[];
                static int rec_elem;

                PktStats (CommandProcessor* cmd_proc, const char* stat_name);
        };

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        bool            interfaceActive;
        Thread*         telemetryPid;
        Thread*         commandPid;

        Publisher*      tlmQ;
        Subscriber*     cmdQ;

        UdpSocket*      tlmSock;
        UdpSocket*      cmdSock;

        bool            dropInvalidPkts;

        // apidStats array initialized to NULLs, created on demand when apid is read
        // last two elements for TLM_APIDS and CMD_APIDS is initialized in constructor
        PktStats*       apidStats[CCSDS_NUM_APIDS + 2];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CfsInterface        (CommandProcessor* cmd_proc, const char* obj_name, const char* tlmq_name, const char* cmdq_name, const char* tlm_ip, int tlm_port, const char* cmd_ip, int cmd_port);
                        ~CfsInterface       (void);

        static void*    telemetryThread     (void* parm);
        static void*    commandThread       (void* parm);

        int             dropInvalidCmd      (int argc, char argv[][MAX_CMD_SIZE]);
        int             logPktStatsCmd      (int argc, char argv[][MAX_CMD_SIZE]);

        PktStats*       createPktStat       (uint16_t apid);
        bool            validatePkt         (unsigned char* pktbuf, int bytes);
        void            measurePkt          (unsigned char* pktbuf, int bytes);
};


#endif  /* __cfs_interface__ */
