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
