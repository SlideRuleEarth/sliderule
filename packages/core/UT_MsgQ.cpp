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
#include "core.h"

/******************************************************************************
 * DEFINES
 ******************************************************************************/

#define MAX_SUBSCRIBERS 15

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_MsgQ::OBJECT_TYPE = "UT_MsgQ";

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
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE)
{
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_MsgQ::~UT_MsgQ(void) = default;

/*----------------------------------------------------------------------------
 * blockingReceiveUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::blockingReceiveUnitTestCmd (lua_State* L) // NOLINT(readability-convert-member-functions-to-static)
{
    UT_MsgQ* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_MsgQ*>(getLuaSelf(L, 1));
        (void)lua_obj; // unused below
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    bool test_status = true;

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
    Publisher* pubq = new Publisher(unit_test_parms.qname, NULL, unit_test_parms.qdepth);

    /* Create Subscriber */
    Subscriber* subq = new Subscriber(unit_test_parms.qname);

    /* STEP 1: Post Data */
    long data = 0;
    for(int i = 0; i < unit_test_parms.qdepth; i++)
    {
        const int status1 = pubq->postCopy(static_cast<void*>(&data), sizeof(long));
        if(status1 <= 0)
        {
            print2term("[%d] ERROR: post %ld error %d\n", __LINE__, data, status1);
            unit_test_parms.errorcnt++;
            break;
        }
        data++;
    }

    /* STEP 2: Verify that Post Times Out */
    const int status2 = pubq->postCopy(static_cast<void*>(&data), sizeof(long), SYS_TIMEOUT);
    if(status2 != MsgQ::STATE_TIMEOUT)
    {
        print2term("[%d] ERROR: post %ld did not timeout: %d\n", __LINE__, data, status2);
        unit_test_parms.errorcnt++;
    }

    /* STEP 3: Receive Data */
    data = 0;
    long value = 0;
    for(int i = 0; i < unit_test_parms.qdepth; i++)
    {
        const int status3 = subq->receiveCopy(static_cast<void*>(&value), sizeof(long), SYS_TIMEOUT);
        if(status3 != sizeof(long))
        {
            print2term("[%d] ERROR: receive failed with status %d\n", __LINE__, status3);
            unit_test_parms.errorcnt++;
        }
        else if(value != data)
        {
            print2term("[%d] ERROR: receive got the wrong value %ld != %ld\n", __LINE__, value, data);
            unit_test_parms.errorcnt++;
        }
        data++;
    }

    /* STEP 4: Verify that Receive Times Out */
    const int status4 = subq->receiveCopy(static_cast<void*>(&value), sizeof(long), SYS_TIMEOUT);
    if(status4 != MsgQ::STATE_TIMEOUT)
    {
        print2term("[%d] ERROR: receive %ld did not timeout: %d\n", __LINE__, data, status4);
        unit_test_parms.errorcnt++;
    }

    if(unit_test_parms.errorcnt != 0) test_status = false;

    /* Clean Up */
    delete pubq;
    delete subq;

    lua_pushboolean(L, test_status);
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
        (void)lua_obj; // unused below
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    bool test_status = true;

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    memset(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_01";
    unit_test_parms.loopcnt = 500;
    unit_test_parms.qdepth = 100;
    unit_test_parms.numpubs = 3;
    unit_test_parms.numsubs = 3;

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
        if(pubparms[p]->errorcnt != 0)
        {
            test_status = false;
        }
        delete pubparms[p]->lastvalue;
        delete pubparms[p];
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        delete s_pid[s];
        if(subparms[s]->errorcnt != 0)
        {
            test_status = false;
            print2term("[%d] ERROR: SUB %d error count is %d\n", __LINE__, s, subparms[s]->errorcnt);
        }
        for(int p = 0; p < unit_test_parms.numpubs; p++)
        {
            if(subparms[s]->lastvalue[p] != 0)
            {
                if(subparms[s]->lastvalue[p] != ((p << 16) | unit_test_parms.loopcnt))
                {
                    test_status = false;
                    print2term("[%d] ERROR: sub %d last value %d of %lX is not %X\n", __LINE__, s, p, subparms[s]->lastvalue[p], (p << 16) | unit_test_parms.loopcnt);
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
                    test_status = false;
                    print2term("[%d] ERROR: msgQ %40s %8d %9s %d failed to unsubscribe all subscribers\n", __LINE__, msgQs[i].name, msgQs[i].len, msgQs[i].state, msgQs[i].subscriptions);
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
    lua_pushboolean(L, test_status);
    return 1;
}

/*----------------------------------------------------------------------------
 * performanceUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::performanceUnitTestCmd (lua_State* L) // NOLINT(readability-convert-member-functions-to-static)
{
    long depth = 500000;
    long size = 1000;
    bool failure = false;

    UT_MsgQ* lua_obj = NULL;
    try
    {
        lua_obj = dynamic_cast<UT_MsgQ*>(getLuaSelf(L, 1));
        (void)lua_obj; // unused below

        depth = getLuaInteger(L, 2, true, depth);
        size = getLuaInteger(L, 3, true, size);
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    /* Create Performance Test Data Structures */
    Publisher* p = new Publisher("testq_03", NULL);
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
            RAW[i].f = false;
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
                print2term("[%d] ERROR: unable to post pkt %d with error %d\n", __LINE__, i, status);
                failure = true;
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
            failure = failure || RAW[i].f; // checks status
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

    lua_pushboolean(L, !failure);
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
        (void)lua_obj; // unused below
    }
    catch(const RunTimeException& e)
    {
        print2term("Failed to get lua parameters: %s", e.what());
        lua_pushboolean(L, false);
        return 1;
    }

    bool test_status = true;

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    memset(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_04";
    unit_test_parms.loopcnt = 5000;
    unit_test_parms.qdepth = 5000;
    unit_test_parms.numpubs = 10;
    unit_test_parms.numsubs = 10;

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
        if(pubparms[p]->errorcnt != 0)
        {
            test_status = false;
        }
        delete pubparms[p]->lastvalue;
        delete pubparms[p];
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        delete s_pid[s];
        if(subparms[s]->errorcnt != 0)
        {
            test_status = false;
            print2term("[%d] ERROR: SUB %d error count is %d\n", __LINE__, s, subparms[s]->errorcnt);
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
                    test_status = false;
                    print2term("[%d] ERROR: msgQ %40s %8d %9s %d failed to unsubscribe all subscribers\n", __LINE__, msgQs[i].name, msgQs[i].len, msgQs[i].state, msgQs[i].subscriptions);
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
    lua_pushboolean(L, test_status);
    return 1;
}

/*----------------------------------------------------------------------------
 * subscriberThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::subscriberThread(void* parm)
{
    parms_t* unit_test_parms = static_cast<parms_t*>(parm);

    /* Initialize Checking Variables */
    unit_test_parms->lastvalue = new long[unit_test_parms->numpubs];
    bool* first_read = new bool[unit_test_parms->numpubs];
    for(int p = 0; p < unit_test_parms->numpubs; p++) first_read[p] = true;

    /* Create Queue */
    randomDelay(100);
    Subscriber* q = new Subscriber(unit_test_parms->qname, MsgQ::SUBSCRIBER_OF_CONFIDENCE, unit_test_parms->qdepth);
    print2term("Subscriber thread %d created on queue %s\n", unit_test_parms->threadid, unit_test_parms->qname);

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
                print2term("[%d] ERROR: out of bounds threadid in %d: %d", __LINE__, unit_test_parms->threadid, threadid);
                unit_test_parms->errorcnt++;
                break;
            }
            else if(first_read[threadid])
            {
                first_read[threadid] = false;
            }
            else if(data != unit_test_parms->lastvalue[threadid] + 1)
            {
                print2term("[%d] ERROR: read %d sequence error %ld != %ld + 1\n", __LINE__, unit_test_parms->threadid, data, unit_test_parms->lastvalue[threadid]);
                unit_test_parms->errorcnt++;
            }
            unit_test_parms->lastvalue[threadid] = data;
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            print2term("Subscriber thread %d encountered timeout\n", unit_test_parms->threadid);
            break;
        }
        else
        {
            print2term("[%d] ERROR: %d error %d\n", __LINE__, unit_test_parms->threadid, status);
            unit_test_parms->errorcnt++;
            break;
        }
    }

    print2term("Subscriber thread %d exited with %d loops to go\n", unit_test_parms->threadid, loops + 1);

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

    /* Last Value Instantiation */
    unit_test_parms->lastvalue = new long;

    /* Create Queue */
    randomDelay(100);
    Publisher* q = new Publisher(unit_test_parms->qname, NULL, unit_test_parms->qdepth);
    print2term("Publisher thread %d created on queue %s\n", unit_test_parms->threadid, unit_test_parms->qname);

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
            print2term("[%d] ERROR: post %d error %d\n", __LINE__, unit_test_parms->threadid, status);
            unit_test_parms->errorcnt++;
            break;
        }
    }

    print2term("Publisher thread %d encountered %d timeouts at data %ld\n", unit_test_parms->threadid, timeout_cnt, data & 0xFFFF);

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
                print2term("[%d] ERROR:  mismatched size of receive: %d != %d\n", __LINE__, ref.size, RAW->size);
                RAW->f = true;
            }
            else
            {
                const unsigned char* pkt = reinterpret_cast<unsigned char*>(ref.data);
                for(int i = 0; i < RAW->size; i++)
                {
                    if(pkt[i] != static_cast<unsigned char>(sequence++))
                    {
                        print2term("[%d] ERROR:  invalid sequence detected in data: %d != %d\n", __LINE__, pkt[i], static_cast<unsigned char>(sequence - 1));
                        RAW->f = true;
                    }
                }
            }

            RAW->s->dereference(ref);
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            print2term("[%d] ERROR:  unexpected timeout on receive at pkt %d!\n", __LINE__, pktnum);
            RAW->f = true;
        }
        else
        {
            print2term("[%d] ERROR:  failed to receive message, error %d\n", __LINE__, status);
            RAW->f = true;
        }
    }

    /* Check Empty */
    Subscriber::msgRef_t ref;
    const int status = RAW->s->receiveRef(ref, IO_CHECK);
    if(status != MsgQ::STATE_EMPTY)
    {
        print2term("[%d] ERROR: queue unexpectedly not empty, return status %d\n", __LINE__, status);
        RAW->f = true;
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
                print2term("[%d] ERROR: out of bounds threadid in %d: %d\n", __LINE__, unit_test_parms->threadid, threadid);
                unit_test_parms->errorcnt++;
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
            print2term("[%d] ERROR: %d error %d\n", __LINE__, unit_test_parms->threadid, status);
            unit_test_parms->errorcnt++;
            break;
        }
        else if(timeouts++ > 1)
        {
            break; // test over
        }
    }

    /* Display Drop Count */
    print2term("Exiting subscriber of opportunity %d test loop at count %d with %d drops\n", unit_test_parms->threadid, loops, drops);

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
