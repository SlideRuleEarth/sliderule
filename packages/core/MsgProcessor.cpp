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

#include "MsgProcessor.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MsgProcessor::OBJECT_TYPE = "MsgProcessor";

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MsgProcessor::MsgProcessor(lua_State* L, const char* inq_name, const char* meta_name, const struct luaL_Reg meta_table[]):
    LuaObject(L, OBJECT_TYPE, meta_name, meta_table)
{
    assert(inq_name);

    /* Initialize Input Stream */
    inQ = new Subscriber(inq_name);

    /* Create Threads */
    processorActive = false;
    thread = NULL;
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
MsgProcessor::~MsgProcessor(void)
{
    // child class resposible for calling destroy method
    delete inQ;
}

/*----------------------------------------------------------------------------
 * initProcessing
 *----------------------------------------------------------------------------*/
bool MsgProcessor::initProcessing(void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * deinitProcessing
 *----------------------------------------------------------------------------*/
bool MsgProcessor::deinitProcessing(void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * handleTimeout
 *----------------------------------------------------------------------------*/
bool MsgProcessor::handleTimeout(void)
{
    return true;
}

/*----------------------------------------------------------------------------
 * isActive
 *----------------------------------------------------------------------------*/
bool MsgProcessor::isActive(void)
{
    return processorActive;
}

/*----------------------------------------------------------------------------
 * isFull
 *----------------------------------------------------------------------------*/
bool MsgProcessor::isFull(void)
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
 * flush
 *----------------------------------------------------------------------------*/
void MsgProcessor::flush(void)
{
    inQ->drain();
}

/*----------------------------------------------------------------------------
 * start
 *----------------------------------------------------------------------------*/
void MsgProcessor::start(void)
{
    processorActive = true;
    thread = new Thread(processorThread, this);
}

/*----------------------------------------------------------------------------
 * stop
 *----------------------------------------------------------------------------*/
void MsgProcessor::stop(void)
{
    processorActive = false;
    delete thread;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processorThread
 *----------------------------------------------------------------------------*/
void* MsgProcessor::processorThread(void* parm)
{
    assert(parm);

    MsgProcessor* processor = (MsgProcessor*)parm;
    bool self_delete = false;

    /* Initialize Processing */
    if(processor->initProcessing() != true)
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
        mlog(CRITICAL, "Fatal error detected in %s, exiting processor\n", processor->getName());
    }

    return NULL;
}

/*----------------------------------------------------------------------------
 * luaDrain
 *----------------------------------------------------------------------------*/
int MsgProcessor::luaDrain (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        MsgProcessor* lua_obj = (MsgProcessor*)getLuaSelf(L, 1);

        /* Drain Queue */
        lua_obj->inQ->drain();

        /* Set Success */
        status = true;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error draining queue: %s\n", e.errmsg);
    }

    /* Return Success */
    return returnLuaStatus(L, status);
}
