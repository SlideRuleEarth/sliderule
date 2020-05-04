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
