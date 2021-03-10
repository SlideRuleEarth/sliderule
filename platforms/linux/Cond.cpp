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
