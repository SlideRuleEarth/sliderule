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

#include "TxTimeProcessor.h"
#include "CommandProcessor.h"

#include "core.h"
#include "legacy.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define MAX_SHOTS 500

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* TxTimeProcessor::TYPE = "TxTimeProcessor";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/

TxTimeProcessor::TxTimeProcessor(CommandProcessor* cmd_proc, const char* obj_name, int pcenum, const char* txtimeq_name):
    CommandableObject(cmd_proc, obj_name, TYPE),
    pce(pcenum)
{
    assert(txtimeq_name);

    /* Initialize Class Components */
    active         = true;
    timesPointer   = 0;
    txtimeq        = new Subscriber(txtimeq_name);
    absoluteTimes  = new long[MAX_SHOTS];
    pthread_create(&pid, NULL, txTimeThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
TxTimeProcessor::~TxTimeProcessor(void)
{
    /* Stop TxTimeProcessor Thread */
    active = false;
    if(pthread_join(pid, NULL) != 0)
    {
        mlog(CRITICAL, "Unable to join TxTime thread: %s\n", strerror(errno));
    }

    /* Delete Log */
    delete txtimeq;
    delete absoluteTimes;

}

/*----------------------------------------------------------------------------
 * createCmd  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* TxTimeProcessor::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* txtimeq_name = StringLib::checkNullStr(argv[0]);
    int         pcenum       = (int)strtol(argv[1], NULL, 0);

    if(txtimeq_name == NULL)
    {
        mlog(CRITICAL, "TxTimeProcessor requires Tx Time Queue name\n");
        return NULL;
    }

    if(pcenum < 1 || pcenum > NUM_PCES)
    {
        mlog(CRITICAL, "Invalid PCE specified: %d, must be between 1 and %d\n", pcenum, NUM_PCES);
        return NULL;
    }

    return new TxTimeProcessor(cmd_proc, name, pcenum, txtimeq_name);
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 *   checkDelta  -
 *----------------------------------------------------------------------------*/
void  TxTimeProcessor::checkDelta(long time)
{
    /* Check that delta has not changed */
    int comparePointer = (timesPointer + 1) % 199;
    long delta = abs(time - absoluteTimes[comparePointer]);
    printf("Delta: %ld\n", delta);
    if (delta != lastDelta)
    {
      mlog(CRITICAL, "[%d] current_time: %ld, compare_time: %ld, delta: %ld\n", pce, time, absoluteTimes[comparePointer], delta);
    }

    lastDelta = delta;

    /* Add new time to array and compute next pointer */
    absoluteTimes[timesPointer++] = time;
    if (timesPointer == MAX_SHOTS) timesPointer = 0;

}

/*----------------------------------------------------------------------------
 *   Tx Time Receiver
 *----------------------------------------------------------------------------*/
void*  TxTimeProcessor::txTimeThread(void* parm)
{
    TxTimeProcessor* txTime = (TxTimeProcessor*)parm;

    if(txTime == NULL || txTime->txtimeq == NULL) return NULL;

    while(txTime->active)
    {
        long absolute_time[1];
        absolute_time[0] = 0;

        /* Block on read of input message queue */
        int size = txTime->txtimeq->receiveCopy(absolute_time, sizeof(long), IO_PEND);

        if(size < 0)
        {
            mlog(ERROR, "Receive in TxTimeProcessor failed with: %d\n", size);
        }
        else
        {
            txTime->checkDelta(absolute_time[0]);
	    }
    }

    return NULL;
}
