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
            processor->inQ->dereference(ref);
        }

        /* Check Status */
        if (!success)
        {
            self_delete = true;
        }
    }

    /* Deinitialize Processing */
    processor->deinitProcessing();

    /* Check Status */
    if (self_delete)
    {
        /* Internally Initiated Exit - Delete Self */
        mlog(CRITICAL, "Fatal error detected in %s, exiting processor\n", processor->getName());
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
