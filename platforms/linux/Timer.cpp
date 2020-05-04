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
        dlog("Unable to create timer... exceeded available signals!\n");
        throw InvalidTimerException();
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
