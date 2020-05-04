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

#ifndef __timer__
#define __timer__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "Mutex.h"

#include <signal.h>
#include <exception>
#include <stdexcept>

/******************************************************************************
 * INVALID TIMER EXCEPTION
 ******************************************************************************/

class InvalidTimerException : public std::runtime_error
{
   public:
       InvalidTimerException() : std::runtime_error("InvalidTimerException") { }
};

/******************************************************************************
 * TIMER CLASS
 ******************************************************************************/

class Timer
{
    public:

        typedef void (*timerHandler_t) (void);

        Timer (timerHandler_t handler, int period_ms);
        ~Timer (void);

    private:

        static const int MAX_TIMERS = 32;
        static int signum;
        static Mutex sigmut;
        static timerHandler_t sighdl[MAX_TIMERS];

        timer_t timerid;
        int     index;

        static void _handler (int sig, siginfo_t *si, void *uc);
};

#endif  /* __timer__ */
