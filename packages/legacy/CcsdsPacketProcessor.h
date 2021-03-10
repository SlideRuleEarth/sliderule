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

#ifndef __ccsds_packet_processor__
#define __ccsds_packet_processor__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CcsdsMsgProcessor.h"
#include "CcsdsProcessorModule.h"
#include "MsgQ.h"
#include "OsApi.h"
#include "List.h"
#include "Dictionary.h"
#include "CcsdsPacket.h"

/******************************************************************************
 * CCSDS PACKET PROCESSOR CLASS
 ******************************************************************************/

class CcsdsPacketProcessor: public CcsdsMsgProcessor
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const unsigned int MAX_WORKERS       = 16;
        static const unsigned int MAX_INT_PERIOD    = 2000;  // 40 seconds of major frames

        static const char* autoFlushKey;
        static const char* autoFlushCntKey;
        static const char* latencyKey;
        static const char* TYPE;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            CcsdsProcessorModule*       processor;
            bool                        enable;
            List<CcsdsSpacePacket*>*    segments;   // allocated at run time
            int                         intpkts;    // full packets (not segments))
            int                         intperiod;  // full packets (not segments))
        } pktProcessor_t;

        typedef struct {
            CcsdsPacketProcessor*       msgproc;
            CcsdsProcessorModule*       processor;
            List<CcsdsSpacePacket*>*    segments;   // passed from pktProcessor_t
            unsigned int                numpkts;
            unsigned int                tries;      // 0 is infinite
            Sem                         runsem;
            Publisher*                  availq;
        } workerThread_t;

        /*--------------------------------------------------------------------
         * Data
         *--------------------------------------------------------------------*/

        int             numWorkerThreads;
        bool            workersActive;

        bool            cmdFlush;
        bool            autoFlush;
        unsigned long   autoFlushCnt;

        bool            dumpErrors;
        bool            measureLatency;
        int64_t         latency;

        Thread**        workerThreads;
        workerThread_t* workerThreadPool; // dynamically allocated array of worker threads
        pktProcessor_t  pktProcessor[CCSDS_NUM_APIDS];

        Subscriber*     subAvailQ;
        Publisher*      pubAvailQ;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

                        CcsdsPacketProcessor    (CommandProcessor* cmd_proc, const char* obj_name, int num_workers, const char* inQ_name);
                        ~CcsdsPacketProcessor   (void);

        int             setAutoFlushCmd         (int argc, char argv[][MAX_CMD_SIZE]);
        int             flushCmd                (int argc, char argv[][MAX_CMD_SIZE]);
        int             filterApidCmd           (int argc, char argv[][MAX_CMD_SIZE]);
        int             integrateApidCmd        (int argc, char argv[][MAX_CMD_SIZE]);
        int             regApidProcCmd          (int argc, char argv[][MAX_CMD_SIZE]);
        int             measureLatencyCmd       (int argc, char argv[][MAX_CMD_SIZE]);
        int             dumpErrorsCmd           (int argc, char argv[][MAX_CMD_SIZE]);

        static void*    workerThread            (void* parm);

        bool            processMsg              (unsigned char* msg, int bytes); // OVERLOAD
        bool            handleTimeout           (void); // OVERLOAD
        bool            resetProcessing         (void);

        static void     freeWorker              (void* obj, void* parm);
};

#endif  /* __ccsds_packet_processor__ */
