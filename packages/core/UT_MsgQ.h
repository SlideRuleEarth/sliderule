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

#include "UnitTest.h"
#include "MsgQ.h"
#include "OsApi.h"

/******************************************************************************
 * CLASS
 ******************************************************************************/

class UT_MsgQ: public UnitTest
{
    public:

        /*--------------------------------------------------------------------
         * Constants
         *--------------------------------------------------------------------*/

        static const char* LUA_META_NAME;
        static const struct luaL_Reg LUA_META_TABLE[];

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        static int  luaCreate   (lua_State* L);

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
            UnitTest* self;     // lua object running test
        } parms_t;

        typedef struct {
            Subscriber* s;
            Sem* v;
            int depth;
            int size;
            UnitTest* self;     // lua object running test
        } perf_thread_t;

        /*--------------------------------------------------------------------
         * Methods
         *--------------------------------------------------------------------*/

        explicit UT_MsgQ (lua_State* L);
        ~UT_MsgQ (void) override = default;

        static int blockingReceiveUnitTestCmd           (lua_State* L);
        static int subscribeUnsubscribeUnitTestCmd      (lua_State* L);
        static int performanceUnitTestCmd               (lua_State* L);
        static int subscriberOfOpporunityUnitTestCmd    (lua_State* L);

        static void* subscriberThread   (void* parm);
        static void* publisherThread    (void* parm);
        static void* performanceThread  (void* parm);
        static void* opportunityThread  (void* parm);

        static void randomDelay (long max_milliseconds);
};

#endif  /* __ut_msgq__ */
