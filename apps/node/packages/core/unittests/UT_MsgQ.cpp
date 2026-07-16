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

#include "UT_MsgQ.h"
#include "UnitTest.h"
#include "OsApi.h"
#include "EventLib.h"
#include "StringLib.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define MAX_SUBSCRIBERS 15

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_MsgQ::LUA_META_NAME = "UT_MsgQ";
const struct luaL_Reg UT_MsgQ::LUA_META_TABLE[] = {
    {"blocking_receive",            blockingReceiveUnitTestCmd},
    {"subscribe_unsubscribe",       subscribeUnsubscribeUnitTestCmd},
    {"performance",                 performanceUnitTestCmd},
    {"subscriber_of_opportunity",   subscriberOfOpporunityUnitTestCmd},
    {NULL,                          NULL}
};

/******************************************************************************
 * METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::luaCreate (lua_State* L)
{
    try
    {
        /* Create Unit Test */
        return createLuaObject(L, new UT_MsgQ(L));
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LUA_META_NAME, e.what());
        return returnLuaStatus(L, false);
    }
}

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
UT_MsgQ::UT_MsgQ (lua_State* L):
    UnitTest(L, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * blockingReceiveUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::blockingReceiveUnitTestCmd (lua_State* L) // NOLINT(readability-convert-member-functions-to-static)
{
    UT_MsgQ* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_MsgQ*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    memset(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_02";
    unit_test_parms.qdepth = 10;
    unit_test_parms.numpubs = 1;
    unit_test_parms.numsubs = 1;

    /* TEST01:
     *      STEP 1: Fill queue
     *      STEP 2: Check blocking post timeout
     *      STEP 3: Check blocking receive
     */

    /* Create Publisher */
    Publisher* pubq = new Publisher(unit_test_parms.qname, unit_test_parms.qdepth);

    /* Create Subscriber */
    Subscriber* subq = new Subscriber(unit_test_parms.qname);

    /* STEP 1: Post Data */
    long data = 0;
    for(int i = 0; i < unit_test_parms.qdepth; i++)
    {
        const int status1 = pubq->postCopy(static_cast<void*>(&data), sizeof(long));
        if(status1 <= 0)
        {
            ut_assert(lua_obj, false, "ERROR: post %ld error %d", data, status1);
            break;
        }
        data++;
    }

    /* STEP 2: Verify that Post Times Out */
    const int status2 = pubq->postCopy(static_cast<void*>(&data), sizeof(long), SYS_TIMEOUT);
    if(status2 != MsgQ::STATE_TIMEOUT)
    {
        ut_assert(lua_obj, false, "ERROR: post %ld did not timeout: %d", data, status2);
    }

    /* STEP 3: Receive Data */
    data = 0;
    long value = 0;
    for(int i = 0; i < unit_test_parms.qdepth; i++)
    {
        const int status3 = subq->receiveCopy(static_cast<void*>(&value), sizeof(long), SYS_TIMEOUT);
        if(status3 != sizeof(long))
        {
            ut_assert(lua_obj, false, "ERROR: receive failed with status %d", status3);
        }
        else if(value != data)
        {
            ut_assert(lua_obj, false, "ERROR: receive got the wrong value %ld != %ld", value, data);
        }
        data++;
    }

    /* STEP 4: Verify that Receive Times Out */
    const int status4 = subq->receiveCopy(static_cast<void*>(&value), sizeof(long), SYS_TIMEOUT);
    if(status4 != MsgQ::STATE_TIMEOUT)
    {
        ut_assert(lua_obj, false, "ERROR: receive %ld did not timeout: %d", data, status4);
    }

    /* Clean Up */
    delete pubq;
    delete subq;

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * subscriberThread  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::subscribeUnsubscribeUnitTestCmd (lua_State* L) // NOLINT(readability-convert-member-functions-to-static)
{
    UT_MsgQ* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_MsgQ*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    ut_initialize(lua_obj);

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    memset(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_01";
    unit_test_parms.loopcnt = 500;
    unit_test_parms.qdepth = 100;
    unit_test_parms.numpubs = 3;
    unit_test_parms.numsubs = 3;
    unit_test_parms.self = lua_obj;

    /* Initialize Random Seed */
    srand(static_cast<unsigned int>(time(NULL)));

    /* Create Thread Data */
    Thread** p_pid = new Thread* [unit_test_parms.numpubs];
    Thread** s_pid = new Thread* [unit_test_parms.numsubs];
    parms_t** pubparms = new parms_t* [unit_test_parms.numpubs];
    parms_t** subparms = new parms_t* [unit_test_parms.numsubs];

    /* Create Threads */
    for(int p = 0; p < unit_test_parms.numpubs; p++)
    {
        pubparms[p] = new parms_t;
        memcpy(pubparms[p], &unit_test_parms, sizeof(parms_t));
        pubparms[p]->threadid = p;
        p_pid[p] = new Thread(publisherThread, static_cast<void*>(pubparms[p]));
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        subparms[s] = new parms_t;
        memcpy(subparms[s], &unit_test_parms, sizeof(parms_t));
        subparms[s]->threadid = s;
        s_pid[s] = new Thread(subscriberThread, static_cast<void*>(subparms[s]));
    }

    /* Join Threads */
    for(int p = 0; p < unit_test_parms.numpubs; p++)
    {
        delete p_pid[p];
        delete pubparms[p]->lastvalue;
        delete pubparms[p];
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        delete s_pid[s];
        for(int p = 0; p < unit_test_parms.numpubs; p++)
        {
            if(subparms[s]->lastvalue[p] != 0)
            {
                if(subparms[s]->lastvalue[p] != ((p << 16) | unit_test_parms.loopcnt))
                {
                    ut_assert(lua_obj, false, "ERROR: sub %d last value %d of %lX is not %X", s, p, subparms[s]->lastvalue[p], (p << 16) | unit_test_parms.loopcnt);
                }
            }
        }
        delete [] subparms[s]->lastvalue;
        delete subparms[s];
    }

    /* Check Results */
    const int numq = MsgQ::numQ();
    if(numq > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[numq];
        const int cnumq = MsgQ::listQ(msgQs, numq);
        for(int i = 0; i < cnumq; i++)
        {
            if(StringLib::match(msgQs[i].name, unit_test_parms.qname))
            {
                if(msgQs[i].subscriptions != 0)
                {
                    ut_assert(lua_obj, false, "ERROR: msgQ %40s %8d %9s %d failed to unsubscribe all subscribers", msgQs[i].name, msgQs[i].len, msgQs[i].state, msgQs[i].subscriptions);
                }
            }
        }
        delete [] msgQs;
    }

    /* Clean Up*/
    delete [] p_pid;
    delete [] s_pid;
    delete [] pubparms;
    delete [] subparms;

    /* Return */
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * performanceUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::performanceUnitTestCmd (lua_State* L) // NOLINT(readability-convert-member-functions-to-static)
{
    long depth = 500000;
    long size = 1000;

    UT_MsgQ* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_MsgQ*>(getLuaSelf(L, 1));
        depth = getLuaInteger(L, 2, true, depth);
        size = getLuaInteger(L, 3, true, size);
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    /* Initialize Test */
    ut_initialize(lua_obj);

    /* Create Performance Test Data Structures */
    Publisher* p = new Publisher("testq_03");
    unsigned long sequence = 0;

    /* Iterate Over Number of Subscribers */
    print2term("Depth, Size, Subscribers, Publishing, Subscribing, Total\n");
    for(int numsubs = 1; numsubs < MAX_SUBSCRIBERS; numsubs++)
    {
        perf_thread_t* RAW = new perf_thread_t[numsubs];

        const clock_t total_start = clock();

        /* Kick-off Subscribers */
        Thread** t = new Thread* [numsubs];
        for(int i = 0; i < numsubs; i++)
        {
            RAW[i].s = new Subscriber("testq_03");
            RAW[i].v = new Sem();
            RAW[i].depth = depth;
            RAW[i].size = size;
            t[i] = new Thread(performanceThread, &RAW[i]);
        }

        /* Publish Packets */
        clock_t start = clock();
        unsigned char* pkt = new unsigned char [size];
        for(int i = 0; i < depth; i++)
        {
            for(int j = 0; j < size; j++)
            {
                pkt[j] = static_cast<unsigned char>(sequence++);
            }

            const int status = p->postCopy(pkt, size);
            if(status <= 0)
            {
                ut_assert(lua_obj, false, "ERROR: unable to post pkt %d with error %d", i, status);
            }
        }
        delete [] pkt;
        clock_t end = clock();
        const double pub_time = ((double) (end - start)) / CLOCKS_PER_SEC;

        /* Start Subscribers */
        start = clock();
        for(int i = 0; i < numsubs; i++)
        {
            RAW[i].v->give();
        }

        /* Join Subscribers */
        for(int i = 0; i < numsubs; i++)
        {
            delete t[i]; // performs a join
        }
        delete [] t;
        end = clock();
        const double sub_time = static_cast<double>(end - start) / CLOCKS_PER_SEC;

        const clock_t total_end = clock();
        const double total_time = static_cast<double>(total_end - total_start) / CLOCKS_PER_SEC;

        /* Print Results */
        print2term("%ld, %ld, %d, %lf, %lf, %lf\n", depth, size, numsubs, pub_time, sub_time, total_time);

        /* Delete RAW */
        for(int i = 0; i < numsubs; i++)
        {
            delete RAW[i].s;
            delete RAW[i].v;
        }
        delete [] RAW;
    }

    /* Delete Test Structures */
    delete p;

    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * subscriberOfOpporunityUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::subscriberOfOpporunityUnitTestCmd (lua_State* L) // NOLINT(readability-convert-member-functions-to-static)
{
    UT_MsgQ* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_MsgQ*>(getLuaSelf(L, 1));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    memset(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_04";
    unit_test_parms.loopcnt = 5000;
    unit_test_parms.qdepth = 5000;
    unit_test_parms.numpubs = 10;
    unit_test_parms.numsubs = 10;
    unit_test_parms.self= lua_obj;

    /* Initialize Test */
    ut_initialize(lua_obj);

    /* Initialize Random Seed */
    srand(static_cast<unsigned int>(time(NULL)));

    /* Create Thread Data */
    Thread** p_pid = new Thread* [unit_test_parms.numpubs];
    Thread** s_pid = new Thread* [unit_test_parms.numsubs];
    parms_t** pubparms = new parms_t* [unit_test_parms.numpubs];
    parms_t** subparms = new parms_t* [unit_test_parms.numsubs];

    /* Create Threads */
    for(int p = 0; p < unit_test_parms.numpubs; p++)
    {
        pubparms[p] = new parms_t;
        memcpy(pubparms[p], &unit_test_parms, sizeof(parms_t));
        pubparms[p]->threadid = p;
        p_pid[p] = new Thread(publisherThread, static_cast<void*>(pubparms[p]));
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        subparms[s] = new parms_t;
        memcpy(subparms[s], &unit_test_parms, sizeof(parms_t));
        subparms[s]->threadid = s;
        s_pid[s] = new Thread(opportunityThread, static_cast<void*>(subparms[s]));
    }

    /* Join Threads */
    for(int p = 0; p < unit_test_parms.numpubs; p++)
    {
        delete p_pid[p];
        delete pubparms[p]->lastvalue;
        delete pubparms[p];
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        delete s_pid[s];
        delete [] subparms[s]->lastvalue;
        delete subparms[s];
    }

    /* Check Results */
    const int numq = MsgQ::numQ();
    if(numq > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[numq];
        const int cnumq = MsgQ::listQ(msgQs, numq);
        for(int i = 0; i < cnumq; i++)
        {
            if(StringLib::match(msgQs[i].name, unit_test_parms.qname))
            {
                if(msgQs[i].subscriptions != 0)
                {
                    ut_assert(lua_obj, false, "ERROR: msgQ %40s %8d %9s %d failed to unsubscribe all subscribers", msgQs[i].name, msgQs[i].len, msgQs[i].state, msgQs[i].subscriptions);
                }
            }
        }
        delete [] msgQs;
    }

    /* Clean Up*/
    delete [] p_pid;
    delete [] s_pid;
    delete [] pubparms;
    delete [] subparms;

    /* Return */
    lua_pushboolean(L, ut_status(lua_obj));
    return 1;
}

/*----------------------------------------------------------------------------
 * subscriberThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::subscriberThread(void* parm)
{
    parms_t* unit_test_parms = static_cast<parms_t*>(parm);
    UnitTest* lua_obj = unit_test_parms->self;

    /* Initialize Checking Variables */
    unit_test_parms->lastvalue = new long[unit_test_parms->numpubs];
    bool* first_read = new bool[unit_test_parms->numpubs];
    for(int p = 0; p < unit_test_parms->numpubs; p++) first_read[p] = true;

    /* Create Queue */
    randomDelay(100);
    Subscriber* q = new Subscriber(unit_test_parms->qname, MsgQ::SUBSCRIBER_OF_CONFIDENCE, unit_test_parms->qdepth);
    mlog(INFO, "Subscriber thread %d created on queue %s", unit_test_parms->threadid, unit_test_parms->qname);

    /* Loop */
    long data;
    int loops = unit_test_parms->loopcnt * unit_test_parms->numpubs;
    while(loops--)
    {
        randomDelay(1);
        const int status = q->receiveCopy(static_cast<void*>(&data), sizeof(long), 1000 * unit_test_parms->numpubs);
        if(status > 0)
        {
            const int threadid = data >> 16;
            if(threadid < 0 || threadid >= unit_test_parms->numpubs)
            {
                ut_assert(lua_obj, false, "ERROR: out of bounds threadid in %d: %d", unit_test_parms->threadid, threadid);
                break;
            }
            else if(first_read[threadid])
            {
                first_read[threadid] = false;
            }
            else if(data != unit_test_parms->lastvalue[threadid] + 1)
            {
                ut_assert(lua_obj, false, "ERROR: read %d sequence error %ld != %ld + 1", unit_test_parms->threadid, data, unit_test_parms->lastvalue[threadid]);
            }
            unit_test_parms->lastvalue[threadid] = data;
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            mlog(INFO, "Subscriber thread %d encountered timeout", unit_test_parms->threadid);
            break;
        }
        else
        {
            ut_assert(lua_obj, false, "ERROR: %d error %d", unit_test_parms->threadid, status);
            break;
        }
    }

    mlog(INFO, "Subscriber thread %d exited with %d loops to go", unit_test_parms->threadid, loops + 1);

    /* Clean Up */
    delete [] first_read;
    delete q;

    return NULL;
}

/*----------------------------------------------------------------------------
 * publisherThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::publisherThread(void* parm)
{
    parms_t* unit_test_parms = static_cast<parms_t*>(parm);
    UnitTest* lua_obj = unit_test_parms->self;

    /* Last Value Instantiation */
    unit_test_parms->lastvalue = new long;

    /* Create Queue */
    randomDelay(100);
    Publisher* q = new Publisher(unit_test_parms->qname, unit_test_parms->qdepth);
    mlog(INFO, "Publisher thread %d created on queue %s", unit_test_parms->threadid, unit_test_parms->qname);

    /* Loop */
    int timeout_cnt = 0;
    long data = (unit_test_parms->threadid << 16) + 1;
    int loops = unit_test_parms->loopcnt;
    while(loops--)
    {
        randomDelay(1);
        const int status = q->postCopy(static_cast<void*>(&data), sizeof(long), 2000 * unit_test_parms->numpubs);
        if(status > 0)
        {
            *unit_test_parms->lastvalue = data++;
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            timeout_cnt++;
        }
        else
        {
            ut_assert(lua_obj, false, "ERROR: post %d error %d", unit_test_parms->threadid, status);
            break;
        }
    }

    mlog(INFO, "Publisher thread %d encountered %d timeouts at data %ld", unit_test_parms->threadid, timeout_cnt, data & 0xFFFF);

    /* Clean Up */
    delete q;

    return NULL;
}

/*----------------------------------------------------------------------------
 * performanceThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::performanceThread(void* parm)
{
    perf_thread_t* RAW = static_cast<perf_thread_t*>(parm);
    UnitTest* lua_obj = RAW->self;
    unsigned long sequence = 0;

    /* Wait to Start */
    RAW->v->take();

    /* Drain Queue */
    for(int pktnum = 0; pktnum < RAW->depth; pktnum++)
    {
        Subscriber::msgRef_t ref;
        const int status = RAW->s->receiveRef(ref, SYS_TIMEOUT);
        if(status > 0)
        {
            if(ref.size != RAW->size)
            {
                ut_assert(lua_obj, false, "ERROR:  mismatched size of receive: %d != %d", ref.size, RAW->size);
            }
            else
            {
                const unsigned char* pkt = reinterpret_cast<unsigned char*>(ref.data);
                for(int i = 0; i < RAW->size; i++)
                {
                    if(pkt[i] != static_cast<unsigned char>(sequence++))
                    {
                        ut_assert(lua_obj, false, "ERROR:  invalid sequence detected in data: %d != %d", pkt[i], static_cast<unsigned char>(sequence - 1));
                    }
                }
            }

            RAW->s->dereference(ref);
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            ut_assert(lua_obj, false, "ERROR:  unexpected timeout on receive at pkt %d!", pktnum);
        }
        else
        {
            ut_assert(lua_obj, false, "ERROR:  failed to receive message, error %d", status);
        }
    }

    /* Check Empty */
    Subscriber::msgRef_t ref;
    const int status = RAW->s->receiveRef(ref, IO_CHECK);
    if(status != MsgQ::STATE_EMPTY)
    {
        ut_assert(lua_obj, false, "ERROR: queue unexpectedly not empty, return status %d", status);
    }

    /* Clean up and exit */
    return NULL;
}

/*----------------------------------------------------------------------------
 * opportunityThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::opportunityThread(void* parm)
{
    parms_t* unit_test_parms = static_cast<parms_t*>(parm);
    UnitTest* lua_obj = unit_test_parms->self;

    /* Initialize Checking Variables */
    unit_test_parms->lastvalue = new long[unit_test_parms->numpubs];
    bool* first_read = new bool[unit_test_parms->numpubs];
    for(int p = 0; p < unit_test_parms->numpubs; p++) first_read[p] = true;

    /* Create Queue */
    randomDelay(100);
    Subscriber* q = new Subscriber(unit_test_parms->qname, MsgQ::SUBSCRIBER_OF_OPPORTUNITY, unit_test_parms->qdepth, MsgQ::CFG_SIZE_INFINITY);

    /* Loop */
    int drops = 0;
    int timeouts = 0;
    long data;
    int loops = unit_test_parms->loopcnt * unit_test_parms->numpubs;
    while(loops--)
    {
        if(loops % 10 == 0) randomDelay(2);
        const int status = q->receiveCopy(static_cast<void*>(&data), sizeof(long), SYS_TIMEOUT);
        if(status > 0)
        {
            const int threadid = data >> 16;
            if(threadid < 0 || threadid >= unit_test_parms->numpubs)
            {
                ut_assert(lua_obj, false, "ERROR: out of bounds threadid in %d: %d", unit_test_parms->threadid, threadid);
                break;
            }
            else if(first_read[threadid])
            {
                first_read[threadid] = false;
            }
            else if(data != unit_test_parms->lastvalue[threadid] + 1)
            {
                drops++;
            }
            unit_test_parms->lastvalue[threadid] = data;
            timeouts = 0;
        }
        else if(status != MsgQ::STATE_TIMEOUT)
        {
            ut_assert(lua_obj, false, "ERROR: %d error %d", unit_test_parms->threadid, status);
            break;
        }
        else if(timeouts++ > 1)
        {
            break; // test over
        }
    }

    /* Display Drop Count */
    mlog(INFO, "Exiting subscriber of opportunity %d test loop at count %d with %d drops", unit_test_parms->threadid, loops, drops);

    /* Clean Up */
    delete [] first_read;
    delete q;

    return NULL;
}

/*----------------------------------------------------------------------------
 * randomDelay  -
 *----------------------------------------------------------------------------*/
void UT_MsgQ::randomDelay(long max_milliseconds)
{
    const long us = rand() % (max_milliseconds * 1000); // NOLINT(concurrency-mt-unsafe)
    OsApi::sleep((double)us / 1000000.0);
}
