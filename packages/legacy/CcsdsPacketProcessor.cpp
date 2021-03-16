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

#include "CcsdsPacketProcessor.h"
#include "CommandProcessor.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsPacketProcessor::autoFlushKey = "autoFlush";
const char* CcsdsPacketProcessor::autoFlushCntKey = "autoFlushCnt";
const char* CcsdsPacketProcessor::latencyKey = "latencyCnt";
const char* CcsdsPacketProcessor::TYPE = "CcsdsPacketProcessor";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* CcsdsPacketProcessor::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    /* Parse Inputs */
    const char*     inq_name        = StringLib::checkNullStr(argv[0]);
    const char*     num_workers_str = argv[1];

    long num_workers = 0;
    if(!StringLib::str2long(num_workers_str, &num_workers))
    {
        mlog(CRITICAL, "Invalid value for number of workers supplied: %s", num_workers_str);
        return NULL;
    }
    else if(num_workers <= 0)
    {
        mlog(CRITICAL, "Invalid number of workers supplied: %ld", num_workers);
        return NULL;
    }

    /* Create Reader */
    return new CcsdsPacketProcessor(cmd_proc, name, num_workers, inq_name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CcsdsPacketProcessor::CcsdsPacketProcessor(CommandProcessor* cmd_proc, const char* obj_name, int num_workers, const char* inq_name):
    CcsdsMsgProcessor(cmd_proc, obj_name, TYPE, inq_name)
{
    /* Initialize Attribute Data */
    cmdFlush        = false;
    autoFlush       = true;
    autoFlushCnt    = 0;
    dumpErrors      = false;
    measureLatency  = false;
    latency         = 0;
    workersActive   = true;

    /* Initialize Number of Workers */
    if(num_workers <= 0)
    {
        mlog(CRITICAL, "invalid number of workers specified: %d, setting to 1", num_workers);
    	numWorkerThreads = 1;
    }
    else if((unsigned int)num_workers > MAX_WORKERS)
    {
        mlog(CRITICAL, "invalid number of workers specified: %d, setting to maximum: %d", num_workers, MAX_WORKERS);
    	numWorkerThreads = MAX_WORKERS;
    }
    else
    {
    	numWorkerThreads = num_workers;
    }

    /* Create and Start Parser Threads */
    pubAvailQ = new Publisher(NULL, freeWorker, numWorkerThreads, sizeof(workerThread_t));
    subAvailQ = new Subscriber(*pubAvailQ);
    workerThreads = new Thread* [numWorkerThreads];
    workerThreadPool = new workerThread_t[numWorkerThreads];
    for(int i = 0; i < numWorkerThreads; i++)
    {
        workerThreadPool[i].msgproc = this;
        workerThreadPool[i].processor = NULL;
        workerThreadPool[i].segments = NULL;
        workerThreadPool[i].numpkts = 0;
        workerThreadPool[i].availq = pubAvailQ;

        workerThreads[i] = new Thread(workerThread, &workerThreadPool[i]);
        pubAvailQ->postRef(&workerThreadPool[i], sizeof(workerThread_t));
    }

    /* Initialize Packet Parsing Data */
    for(int i = 0; i < CCSDS_NUM_APIDS; i++)
    {
        pktProcessor[i].processor = NULL;
        pktProcessor[i].enable = false;
        pktProcessor[i].segments = NULL;
        pktProcessor[i].intpkts = 0;
        pktProcessor[i].intperiod = 1;
    }

    /* Register Current Values */
    cmdProc->setCurrentValue(getName(), autoFlushKey,       (void*)&autoFlush,      sizeof(autoFlush));
    cmdProc->setCurrentValue(getName(), autoFlushCntKey,    (void*)&autoFlushCnt,   sizeof(autoFlushCnt));
    cmdProc->setCurrentValue(getName(), latencyKey,         (void*)&latency,        sizeof(latency));

    /* Register Commands */
    registerCommand("SET_AUTO_FLUSH", (cmdFunc_t)&CcsdsPacketProcessor::setAutoFlushCmd,    1, "<ENABLE|DISABLE>");
    registerCommand("FLUSH",          (cmdFunc_t)&CcsdsPacketProcessor::flushCmd,           0, "");
    registerCommand("FILTER",         (cmdFunc_t)&CcsdsPacketProcessor::filterApidCmd,      2, "<ENABLE|DISABLE> <apid>");
    registerCommand("INTEGRATE",      (cmdFunc_t)&CcsdsPacketProcessor::integrateApidCmd,   2, "<apid> <integration period>");
    registerCommand("REGISTER",       (cmdFunc_t)&CcsdsPacketProcessor::regApidProcCmd,     2, "<apid> <processor object name>");
    registerCommand("MEASURE_LATENCY",(cmdFunc_t)&CcsdsPacketProcessor::measureLatencyCmd,  1, "<ENABLE|DISABLE>");
    registerCommand("DUMP_ERRORS",    (cmdFunc_t)&CcsdsPacketProcessor::dumpErrorsCmd,      1, "<ENABLE|DISABLE>");

    /* Start Processor */
    start();
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CcsdsPacketProcessor::~CcsdsPacketProcessor(void)
{
    workersActive = false; // stop workers
    stop(); // stop processor

    if(resetProcessing())
    {
        delete pubAvailQ;
        delete subAvailQ;
        for(int i = 0; i < numWorkerThreads; i++)
        {
            delete workerThreads[i];
        }
        delete [] workerThreads;
        delete [] workerThreadPool;
    }
}

/*----------------------------------------------------------------------------
 * setAutoFlushCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::setAutoFlushCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    autoFlush = enable;
    cmdProc->setCurrentValue(getName(), autoFlushKey, (void*)&enable, sizeof(enable));

    return 0;
}

/*----------------------------------------------------------------------------
 * flushCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::flushCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    cmdFlush = true;

    return 0;
}

/*----------------------------------------------------------------------------
 * filterApidCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::filterApidCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    long apid = 0;
    if(!StringLib::str2long(argv[1], &apid))
    {
        mlog(CRITICAL, "Invalid APID supplied: %s", argv[1]);
        return -1;
    }

    if(apid >= 0 && apid < CCSDS_NUM_APIDS)
    {
        if(enable && !pktProcessor[apid].processor)
        {
            mlog(CRITICAL, "APID %04X has no registered processor!", (uint16_t)apid);
            return -1;
        }

        pktProcessor[apid].enable = enable;
    }
    else if(apid == ALL_APIDS)
    {
        for(int i = 0; i < CCSDS_NUM_APIDS; i++)
        {
            if(enable && !pktProcessor[i].processor)
            {
                mlog(CRITICAL, "APID %04X has no registered processor!", i);
                return -1;
            }

            pktProcessor[i].enable = enable;
        }
    }
    else
    {
        mlog(CRITICAL, "Invalid APID specified: %04X", (uint16_t)apid);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * integrateApidCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::integrateApidCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    long apid = 0;
    if(!StringLib::str2long(argv[0], &apid))
    {
        mlog(CRITICAL, "Invalid APID supplied: %s", argv[0]);
        return -1;
    }

    long int_period = 0;
    if(!StringLib::str2long(argv[1], &int_period))
    {
        mlog(CRITICAL, "Invalid integration period supplied: %s", argv[1]);
        return -1;
    }

    if(apid >= 0 && apid < CCSDS_NUM_APIDS)
    {
        pktProcessor[apid].intperiod = int_period;
    }
    else if(apid == ALL_APIDS)
    {
        for(int i = 0; i < CCSDS_NUM_APIDS; i++)
        {
            pktProcessor[i].intperiod = int_period;
        }
    }
    else
    {
        mlog(CRITICAL, "Invalid APID specified: %04X", (uint16_t)apid);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * measureLatencyCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::measureLatencyCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    measureLatency = enable;

    return 0;
}

/*----------------------------------------------------------------------------
 * dumpErrorsCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::dumpErrorsCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    bool enable;
    if(!StringLib::str2bool(argv[0], &enable)) return -1;

    dumpErrors = enable;

    return 0;
}

/*----------------------------------------------------------------------------
 * regApidProcCmd
 *----------------------------------------------------------------------------*/
int CcsdsPacketProcessor::regApidProcCmd(int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    /* Get APID */
    long apid = 0;
    if(!StringLib::str2long(argv[0], &apid))
    {
        mlog(CRITICAL, "Invalid APID supplied: %s", argv[0]);
        return -1;
    }

    /* Get Processor Object Name */
    const char* proc_obj_name = argv[1];

    /* Set Processor */
    CcsdsProcessorModule* processor = (CcsdsProcessorModule*)cmdProc->getObject(proc_obj_name, "CcsdsProcessorModule");
    if(processor == NULL)
    {
        mlog(CRITICAL, "Unable to find processor module %s", proc_obj_name);
        return -1;
    }
    else if(apid >= 0 && apid < CCSDS_NUM_APIDS)
    {
        if(pktProcessor[apid].processor == NULL)
        {
            pktProcessor[apid].processor = processor;
            pktProcessor[apid].enable = true;
        }
        else
        {
            mlog(ERROR, "Packet processor %s for APID %04X already set!", getName(), (uint16_t)apid);
        }
    }
    else if(apid == ALL_APIDS)
    {
        for(int i = 0; i < CCSDS_NUM_APIDS; i++)
        {
            if(pktProcessor[i].processor == NULL)
            {
                pktProcessor[i].processor = processor;
                pktProcessor[i].enable = true;
            }
            else
            {
                mlog(ERROR, "Packet processor %s for APID %04X already set!", getName(), i);
            }
        }
    }
    else
    {
        mlog(CRITICAL, "Invalid APID specified: %04X", (uint16_t)apid);
        return -1;
    }

    return 0;
}

/*----------------------------------------------------------------------------
 * workerThread  -
 *----------------------------------------------------------------------------*/
void* CcsdsPacketProcessor::workerThread (void* parm)
{
    workerThread_t* worker = (workerThread_t*)parm;

    while(worker->msgproc->workersActive)
    {
        /* Wait for Work to Do */
        while(!worker->runsem.take(SYS_TIMEOUT) && worker->msgproc->workersActive);
        if(!worker->msgproc->workersActive) break;

        /* Process Packet Segments */
        if(worker->processor->processSegments(*(worker->segments), worker->numpkts) == false)
        {
            mlog(ERROR, "%s failed to process packet, packet dropped", worker->processor->getName());
            if(worker->msgproc->dumpErrors)
            {
                for(int s = 0; s < worker->segments->length(); s++)
                {
                    CcsdsPacket* seg = worker->segments->get(s);
                    int seglen = seg->getLEN();
                    unsigned char* segbuf = seg->getBuffer();
                    print2term("[%d]: ", seglen);
                    for(int i = 0; i < seglen; i++) print2term("%02X", segbuf[i]);
                    print2term("\n");
                }
            }
        }

        /* Delete Packets */
        for(int s = 0; s < worker->segments->length(); s++)
        {
            delete worker->segments->get(s); // deletes malloc'ed CcsdsPacket in processMsg
        }
        delete worker->segments; // deletes malloc'ed List<CcsdsPacket*> in processMsg
        worker->segments = NULL; // informs resetProcessing() that segments has been freed

        /* Make Available Again */
        int status = worker->availq->postRef(worker, sizeof(workerThread_t));
        if(status <= 0)
        {
            mlog(CRITICAL, "Failed to post available worker ...exiting thread!");
            break;
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * processMsg  - virtual processing function for StreamProcessor class
 *----------------------------------------------------------------------------*/
bool CcsdsPacketProcessor::processMsg (unsigned char* msg, int bytes)
{
    /* Sanity Check Packet */
    if(msg == NULL)
    {
        mlog(CRITICAL, "Null message passed to %s", getName());
        return false; // this is not expected to occur in correct code
    }
    if(bytes < CCSDS_SPACE_HEADER_SIZE)
    {
        mlog(CRITICAL, "Length too small (%d < %d) on CCSDS packet provided to %s", bytes, CCSDS_SPACE_HEADER_SIZE, getName());
        return true; // packet still handled, no need to kill self
    }

    /* Create Packet */
    CcsdsSpacePacket* pkt = NULL;
    try
    {
        pkt = new CcsdsSpacePacket(msg, bytes, true); // memory allocated here
    }
    catch (std::invalid_argument& e)
    {
        mlog(CRITICAL, "Unable to create CCSDS packet from buffer: %s", e.what());
        return false;
    }

    /* Pull Out CCSDS Header Parameters */
    uint16_t apid = pkt->getAPID();
    uint16_t len  = pkt->getLEN();
    CcsdsSpacePacket::seg_flags_t seg = pkt->getSEQFLG();

    /* Check if Enabled */
    if(pktProcessor[apid].enable)
    {
        /* Check Length */
        if(len != bytes)
        {
            mlog(CRITICAL, "Length mismatch on CCSDS packet %04X provided to %s: %d != %d", apid, getName(), len, bytes);
            return true; // packet still handled, no need to kill self
        }

        /* Buffer Segment */
        if(pktProcessor[apid].segments == NULL)
        {
            pktProcessor[apid].segments = new List<CcsdsSpacePacket*>();
        }
        pktProcessor[apid].segments->add(pkt);

        /* Process Packet */
        if(seg == CcsdsSpacePacket::SEG_NONE || seg == CcsdsSpacePacket::SEG_STOP)
        {
            /* Check If Fully Integrated */
            pktProcessor[apid].intpkts++;
            if(pktProcessor[apid].intpkts >= pktProcessor[apid].intperiod)
            {
                pktProcessor[apid].intpkts = 0;

                /* Pull Out Time */
                if(measureLatency)
                {
                    int64_t nowt = TimeLib::gettimems();
                    int64_t pktt = TimeLib::gmt2gpstime(TimeLib::cds2gmttime(CCSDS_GET_CDS_DAYS(msg), CCSDS_GET_CDS_MSECS(msg)));
                    latency = nowt - pktt;
                    cmdProc->setCurrentValue(getName(), latencyKey, (void*)&latency, sizeof(latency));
                }

                /* Grab Available Worker */
                Subscriber::msgRef_t ref;
                int status = subAvailQ->receiveRef(ref, 5000); // wait for five seconds
                if(status > 0)
                {
                    subAvailQ->dereference(ref, false); // free up memory in message queue, but don't delete the worker
                    workerThread_t* worker = (workerThread_t*)ref.data;

                    /* Configure Worker */
                    worker->processor   = pktProcessor[apid].processor;
                    worker->segments    = pktProcessor[apid].segments;
                    worker->numpkts     = pktProcessor[apid].intperiod;

                    /* Reset Segment List */
                    pktProcessor[apid].segments = NULL;

                    /* Kick Off Worker */
                    worker->runsem.give();
                }
                else
                {
                    mlog(CRITICAL, "%s failed to get available worker!", getName());
                }
            }
        }
    }

    /* Check for Command Flush */
    if(cmdFlush)
    {
        resetProcessing();
    }

    return true;
}

/*----------------------------------------------------------------------------
 * handleTimeout  -
 *----------------------------------------------------------------------------*/
bool CcsdsPacketProcessor::handleTimeout (void)
{
    if((autoFlush && isFull()) || cmdFlush)
    {
        autoFlushCnt++;
        cmdProc->setCurrentValue(getName(), autoFlushCntKey, (void*)&autoFlushCnt, sizeof(autoFlushCnt));
        resetProcessing(); // no check of return status because returning false is fatal to message processor parent class
    }

    return true;
}

/*----------------------------------------------------------------------------
 * resetProcessing
 *----------------------------------------------------------------------------*/
bool CcsdsPacketProcessor::resetProcessing (void)
{
    /* Reset Command Flush */
    cmdFlush = false;

    /* Empty Out Input Queue */
    flush();

    /* Wait for all workers to finish */
    int worker_check = 5;
    while( (worker_check-- > 0) && (subAvailQ->getCount() != numWorkerThreads)) LocalLib::sleep(1);

    /* Clear out stored packets in parsers */
    if(subAvailQ->getCount() == numWorkerThreads)
    {
        for(int apid = 0; apid < CCSDS_NUM_APIDS; apid++)
        {
            if(pktProcessor[apid].enable && pktProcessor[apid].segments)
            {
                for(int seg=0; seg < pktProcessor[apid].segments->length(); seg++)
                {
                    delete pktProcessor[apid].segments->get(seg);
                }
                pktProcessor[apid].segments->clear();
                delete pktProcessor[apid].segments;
            }
        }
    }
    else
    {
        mlog(CRITICAL, "unable to flush packet queue as all workers did not complete in time allowed: %d of %d", subAvailQ->getCount(), numWorkerThreads);
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------
 * freeWorker
 *----------------------------------------------------------------------------*/
void CcsdsPacketProcessor::freeWorker (void* obj, void* parm)
{
    (void)parm;
    workerThread_t* worker = (workerThread_t*)obj;

    // -- DO NOT DELETE ... this memory is deallocated in the delete [] of workerThreadPool
    (void) worker;
}
