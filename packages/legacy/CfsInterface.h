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
