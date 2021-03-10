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

#ifndef __ut_msgq__
#define __ut_msgq__

/******************************************************************************
 * INCLUDES
 ******************************************************************************/

#include "CommandableObject.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * UNIT TEST MSGQ CLASS
 ******************************************************************************/

class UT_MsgQ: public CommandableObject
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* TYPE;
        static const int MAX_SUBSCRIBERS = 15;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static CommandableObject* createObject (CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE]);

    private:

        /*--------------------------------------------------------------------
         * Typedefs
         *--------------------------------------------------------------------*/

        typedef struct {
            const char* qname;  // test queue name
            int loopcnt;        // number of times to loop through publishing
            int numpubs;        // number of publishers
            int numsubs;        // number of subscribers
            int threadid;       // identification for thread
            long* lastvalue;    // array of previous values read by subscriber, indexed by threadid
            int qdepth;         // size of the queue - number of elements it can hold
            int errorcnt;
        } parms_t;

        typedef struct {
            Subscriber* s;
            Sem* v;
            bool f;
            int depth;
            int size;
        } perf_thread_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        UT_MsgQ (CommandProcessor* cmd_proc, const char* obj_name);
        ~UT_MsgQ (void);

        int blockingReceiveUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE]);
        int subscribeUnsubscribeUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE]);
        int performanceUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE]);
        int subscriberOfOpporunityUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE]);

        static void* subscriberThread (void* parm);
        static void* publisherThread (void* parm);
        static void* performanceThread (void* parm);
        static void* opportunityThread (void* parm);

        static void randomDelay(long max_milliseconds);
};

#endif  /* __ut_msgq__ */
