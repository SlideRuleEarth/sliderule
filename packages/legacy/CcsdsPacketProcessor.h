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
