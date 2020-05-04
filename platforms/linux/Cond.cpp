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

#include "OsApi.h"
#include "TimeLib.h"

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Cond::Cond(int num_sigs): Mutex()
{
    assert(num_sigs > 0);
    numSigs = num_sigs;
    condId = new pthread_cond_t[numSigs];
    for(int i = 0; i < numSigs; i++)
    {
        pthread_cond_init(&condId[i], NULL);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Cond::~Cond()
{
    for(int i = 0; i < numSigs; i++)
    {
        pthread_cond_destroy(&condId[i]);
    }
    delete [] condId;
}

/*----------------------------------------------------------------------------
 * signal
 *----------------------------------------------------------------------------*/
void Cond::signal(int sig, notify_t notify)
{
    assert(sig < numSigs);
    if(notify == NOTIFY_ALL)    pthread_cond_broadcast(&condId[sig]);
    else                        pthread_cond_signal(&condId[sig]);
}

/*----------------------------------------------------------------------------
 * wait
 *----------------------------------------------------------------------------*/
bool Cond::wait(int sig, int timeout_ms)
{
    assert(sig < numSigs);
    
    int status;

    /* Perform Wait */
    if(timeout_ms == IO_PEND)
    {
        /* Block Forever until Success */
        status = pthread_cond_wait(&condId[sig], &mutexId);
    }
    else if(timeout_ms > 0)
    {
        /* Build Time Structure */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec  += (time_t) (timeout_ms / 1000);
        ts.tv_nsec +=  (timeout_ms % 1000) * 1000000L;
        if(ts.tv_nsec  >= 1000000000L)
        {
            ts.tv_nsec -= 1000000000L;
            ts.tv_sec++;
        }

        /* Block on Timed Wait */
        status = pthread_cond_timedwait(&condId[sig], &mutexId, &ts);
    }
    else // timeout_ms = 0
    {
        // note that NON-BLOKCING CHECK is an error since the pthread
        // conditional does not support a non-blocking attempt
        status = PARM_ERR_RC;
    }

    /* Return Status */
    if(status == 0) return true;
    else            return false;
}
