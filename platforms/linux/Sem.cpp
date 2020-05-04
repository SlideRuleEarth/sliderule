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

#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <semaphore.h>
#include <pthread.h>

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Sem::Sem()
{
    sem_init(&semId, 0, 0);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Sem::~Sem()
{
    sem_destroy(&semId);
}

/*----------------------------------------------------------------------------
 * give
 *----------------------------------------------------------------------------*/
void Sem::give(void)
{
    sem_post(&semId);
}

/*----------------------------------------------------------------------------
 * take
 *----------------------------------------------------------------------------*/
bool Sem::take(int timeout_ms)
{
    int status = 1;

    /* Perform Take */
    if(timeout_ms > 0)
    {
        /* Build Time Structure */
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec  += (time_t) (timeout_ms / 1000) ;
        ts.tv_nsec +=  (timeout_ms % 1000) * 1000000L ;
        if(ts.tv_nsec  >= 1000000000L )
        {
            ts.tv_nsec -= 1000000000L ;
            ts.tv_sec++ ;
        }

        /* Loop until Timeout or Success */
        do {
            status = sem_timedwait(&semId, &ts);
        } while(status == -1 && errno == EINTR);
    }
    else if(timeout_ms == IO_CHECK)
    {
        /* Non-Blocking Attempt */
        do {
            status = sem_trywait(&semId);
        } while(status == -1 && errno == EINTR);                
    }
    else if(timeout_ms == IO_PEND)
    {
        /* Block Forever until Success */
        do {
            status = sem_wait(&semId);
        } while(status == -1 && errno == EINTR);                
    }
    else
    {
        status = PARM_ERR_RC;
    }
    
    /* Return Status */
    if(status == 0) return true;
    else            return false;
}
