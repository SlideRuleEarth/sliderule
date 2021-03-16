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

#include <time.h>
#include <limits.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <netinet/in.h>
#include <exception>
#include <stdexcept>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

Mutex Timer::sigmut;
int Timer::signum = 0;
Timer::timerHandler_t Timer::sighdl[MAX_TIMERS] = {0};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Timer::Timer(timerHandler_t handler, int period_ms)
{
    /* Get Signal ID */
    int sigid = -1;
    sigmut.lock();
    {
        if(signum < (SIGRTMAX - SIGRTMIN))
        {
            for(int i = 0; i < MAX_TIMERS; i++)
            {
                if(sighdl[i] == NULL)
                {
                    index = i;
                    sighdl[index] = handler;
                    signum++;
                    sigid = SIGRTMIN + index;
                    break;
                }
            }
        }
    }
    sigmut.unlock();
    
    /* Set Timer */
    if(sigid != -1)
    {
        struct sigevent sev;
        struct itimerspec its;
        sigset_t mask;
        struct sigaction sa;

        /* Establish handler for timer signal */
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = _handler;
        sigemptyset(&sa.sa_mask);
        sigaction(sigid, &sa, NULL);

        /* Block timer signal temporarily */
        sigemptyset(&mask);
        sigaddset(&mask, sigid);
        sigprocmask(SIG_SETMASK, &mask, NULL);

        /* Create the timer */
        sev.sigev_notify = SIGEV_SIGNAL;
        sev.sigev_signo = sigid;
        sev.sigev_value.sival_ptr = &timerid;
        timer_create(CLOCK_REALTIME, &sev, &timerid);

        /* Start the timer */
        its.it_value.tv_sec = period_ms / 1000;
        its.it_value.tv_nsec = period_ms * 1000000;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;
        timer_settime(timerid, 0, &its, NULL);
        sigprocmask(SIG_UNBLOCK, &mask, NULL);
    }
    else
    {
        dlog("Unable to create timer... exceeded available signals!");
        throw RunTimeException("unable to create timer");
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Timer::~Timer()
{
    /* Unset Signal ID */
    sigmut.lock();
    {
        sighdl[index] = NULL;
        signum--;
    }
    sigmut.unlock();

    /* Delete Timer */
    timer_delete(timerid);
}

/*----------------------------------------------------------------------------
 * _handler
 *----------------------------------------------------------------------------*/
void Timer::_handler (int sig, siginfo_t *si, void *uc)
{
    (void)si;
    (void)uc;
    
    sighdl[sig - SIGRTMIN]();
}
