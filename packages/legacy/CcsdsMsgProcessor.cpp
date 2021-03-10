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

#include "CcsdsMsgProcessor.h"
#include "CommandProcessor.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CcsdsMsgProcessor::CcsdsMsgProcessor(CommandProcessor* cmd_proc, const char* obj_name, const char* _type, const char* inq_name):
    CommandableObject(cmd_proc, obj_name, _type)
{
    assert(inq_name);

    /* Initialize Input Stream */
    inQ = new Subscriber(inq_name);

    /* Register Commands */
    registerCommand("DRAIN", (cmdFunc_t)&CcsdsMsgProcessor::drainCmd, 0, "");

    /* Create Threads */
    processorActive = false;
    thread = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CcsdsMsgProcessor::~CcsdsMsgProcessor(void)
{
    // child class resposible for calling destroy method
    delete inQ;
}

/*----------------------------------------------------------------------------
 * initProcessing  -
 *----------------------------------------------------------------------------*/
bool CcsdsMsgProcessor::initProcessing(void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * deinitProcessing  -
 *----------------------------------------------------------------------------*/
bool CcsdsMsgProcessor::deinitProcessing(void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * handleTimeout  -
 *----------------------------------------------------------------------------*/
bool CcsdsMsgProcessor::handleTimeout(void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * isActive  -
 *----------------------------------------------------------------------------*/
bool CcsdsMsgProcessor::isActive(void)
{
    return processorActive;
}

/*----------------------------------------------------------------------------
 * isFull  -
 *----------------------------------------------------------------------------*/
bool CcsdsMsgProcessor::isFull(void)
{
    int pkts_in_q = inQ->getCount();
    int space_in_q = inQ->getDepth();
    if (pkts_in_q > 0 && pkts_in_q == space_in_q)
    {
        return true;
    }
    else
    {
        return false;
    }
}

/*----------------------------------------------------------------------------
 * flush  -
 *----------------------------------------------------------------------------*/
void CcsdsMsgProcessor::flush(void)
{
    inQ->drain();
}

/*----------------------------------------------------------------------------
 * start  -
 *----------------------------------------------------------------------------*/
void CcsdsMsgProcessor::start(void)
{
    processorActive = true;
    thread = new Thread(processorThread, this);
}

/*----------------------------------------------------------------------------
 * stop  -
 *----------------------------------------------------------------------------*/
void CcsdsMsgProcessor::stop(void)
{
    processorActive = false;
    delete thread;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processorThread  -
 *----------------------------------------------------------------------------*/
void* CcsdsMsgProcessor::processorThread(void* parm)
{
    assert(parm);

    CcsdsMsgProcessor* processor = (CcsdsMsgProcessor*)parm;
    bool self_delete = false;

    /* Initialize Processing */
    if (processor->initProcessing() != true)
    {
        self_delete = true;
    }

    /* Loop While Read Active */
    while(processor->processorActive && !self_delete)
    {
        /* Read Bytes */
        Subscriber::msgRef_t ref;
        int status = processor->inQ->receiveRef(ref, SYS_TIMEOUT);

        /* Process Bytes */
        bool success = false;
        if(status == MsgQ::STATE_TIMEOUT)
        {
            success = processor->handleTimeout();
        }
        else if(status > 0)
        {
            if(ref.size > 0)
            {
                success = processor->processMsg((unsigned char*)ref.data, ref.size);
            }
            else /* terminator */
            {
                success = true;
                self_delete = true;
            }
            processor->inQ->dereference(ref);
        }

        /* Check Status */
        if (!success)
        {
            mlog(CRITICAL, "Fatal error detected in %s, exiting processor\n", processor->getName());
            self_delete = true;
        }
    }

    /* Deinitialize Processing */
    processor->deinitProcessing();

    /* Check Status */
    if (self_delete)
    {
        /* Internally Initiated Exit - Delete Self */
        processor->cmdProc->deleteObject(processor->getName());
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * drainCmd
 *----------------------------------------------------------------------------*/
int CcsdsMsgProcessor::drainCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    inQ->drain();

    return 0;
}
