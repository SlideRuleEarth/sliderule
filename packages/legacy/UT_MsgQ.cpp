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

#include "UT_MsgQ.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* UT_MsgQ::TYPE = "UT_MsgQ";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *----------------------------------------------------------------------------*/
CommandableObject* UT_MsgQ::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    /* Create Message Queue Unit Test */
    return new UT_MsgQ(cmd_proc, name);
}

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
UT_MsgQ::UT_MsgQ(CommandProcessor* cmd_proc, const char* obj_name): 
    CommandableObject(cmd_proc, obj_name, TYPE)
{
    /* Register Commands */
    registerCommand("BLOCKING_RECEIVE_TEST", (cmdFunc_t)&UT_MsgQ::blockingReceiveUnitTestCmd, 0, "");
    registerCommand("SUBSCRIBE_UNSUBSCRIBE_TEST", (cmdFunc_t)&UT_MsgQ::subscribeUnsubscribeUnitTestCmd, 0, "");
    registerCommand("PERFORMANCE_TEST", (cmdFunc_t)&UT_MsgQ::performanceUnitTestCmd, 0, "[<depth> <size>]");
    registerCommand("SUBSCRIBER_OF_OPPORTUNITY_TEST", (cmdFunc_t)&UT_MsgQ::subscriberOfOpporunityUnitTestCmd, 0, "");    
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
UT_MsgQ::~UT_MsgQ(void)
{
}

/*----------------------------------------------------------------------------
 * blockingReceiveUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::blockingReceiveUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    bool test_status = true;

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    LocalLib::set(&unit_test_parms, 0, sizeof(unit_test_parms));
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
        int status1 = pubq->postCopy((void*)&data, sizeof(long));
        if(status1 <= 0)
        {
            mlog(RAW, "[%d] ERROR: post %ld error %d\n", __LINE__, data, status1);
            unit_test_parms.errorcnt++;
            break;
        }
        data++;
    }

    /* STEP 2: Verify that Post Times Out */
    int status2 = pubq->postCopy((void*)&data, sizeof(long), SYS_TIMEOUT);
    if(status2 != MsgQ::STATE_TIMEOUT)
    {
        mlog(RAW, "[%d] ERROR: post %ld did not timeout: %d\n", __LINE__, data, status2);
        unit_test_parms.errorcnt++;
    }

    /* STEP 3: Receive Data */
    data = 0;
    long value = 0;
    for(int i = 0; i < unit_test_parms.qdepth; i++)
    {
        int status3 = subq->receiveCopy((void*)&value, sizeof(long), SYS_TIMEOUT);
        if(status3 != sizeof(long))
        {
            mlog(RAW, "[%d] ERROR: receive failed with status %d\n", __LINE__, status3);
            unit_test_parms.errorcnt++;
        }
        else if(value != data)
        {
            mlog(RAW, "[%d] ERROR: receive got the wrong value %ld != %ld\n", __LINE__, value, data);
            unit_test_parms.errorcnt++;            
        }
        data++;
    }

    /* STEP 4: Verify that Receive Times Out */
    int status4 = subq->receiveCopy((void*)&value, sizeof(long), SYS_TIMEOUT);
    if(status4 != MsgQ::STATE_TIMEOUT)
    {
        mlog(RAW, "[%d] ERROR: receive %ld did not timeout: %d\n", __LINE__, data, status4);
        unit_test_parms.errorcnt++;
    }

    if(unit_test_parms.errorcnt != 0) test_status = false;
    
    /* Clean Up */
    delete pubq;
    delete subq;

    if(test_status) return 0;
    else            return -1;
}

/*----------------------------------------------------------------------------
 * subscriberThread  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::subscribeUnsubscribeUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    bool test_status = true;

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    LocalLib::set(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_01";
    unit_test_parms.loopcnt = 500;
    unit_test_parms.qdepth = 100;
    unit_test_parms.numpubs = 3;
    unit_test_parms.numsubs = 3;

    /* Initialize Random Seed */
    srand((unsigned int)time(NULL));

    /* Create Thread Data */
    Thread** p_pid = new Thread* [unit_test_parms.numpubs];
    Thread** s_pid = new Thread* [unit_test_parms.numsubs];
    parms_t** pubparms = new parms_t* [unit_test_parms.numpubs];
    parms_t** subparms = new parms_t* [unit_test_parms.numsubs];

    /* Create Threads */
    for(int p = 0; p < unit_test_parms.numpubs; p++)
    {
        pubparms[p] = new parms_t;
        LocalLib::copy(pubparms[p], &unit_test_parms, sizeof(parms_t));
        pubparms[p]->threadid = p;
        p_pid[p] = new Thread(publisherThread, (void*)pubparms[p]);
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        subparms[s] = new parms_t;
        LocalLib::copy(subparms[s], &unit_test_parms, sizeof(parms_t));
        subparms[s]->threadid = s;
        s_pid[s] = new Thread(subscriberThread, (void*)subparms[s]);
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
            mlog(RAW, "[%d] ERROR: SUB %d error count is %d\n", __LINE__, s, subparms[s]->errorcnt);
        }
        for(int p = 0; p < unit_test_parms.numpubs; p++)
        {
            if(subparms[s]->lastvalue[p] != 0)
            {
                if(subparms[s]->lastvalue[p] != ((p << 16) | unit_test_parms.loopcnt))
                {
                    test_status = false;
                    mlog(RAW, "[%d] ERROR: sub %d last value %d of %lX is not %X\n", __LINE__, s, p, subparms[s]->lastvalue[p], (p << 16) | unit_test_parms.loopcnt);
                }
            }
        }
        delete [] subparms[s]->lastvalue;
        delete subparms[s];
    }

    /* Check Results */
    int numq = MsgQ::numQ();
    if(numq > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[numq];
        int cnumq = MsgQ::listQ(msgQs, numq);
        for(int i = 0; i < cnumq; i++)
        {
            if(StringLib::match(msgQs[i].name, unit_test_parms.qname))
            {
                if(msgQs[i].subscriptions != 0)
                {
                    test_status = false;
                    mlog(RAW, "[%d] ERROR: msgQ %40s %8d %9s %d failed to unsubscribe all subscribers\n", __LINE__, msgQs[i].name, msgQs[i].len, msgQs[i].state, msgQs[i].subscriptions);
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
    if(test_status) return 0;
    else            return -1;
}

/*----------------------------------------------------------------------------
 * performanceUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::performanceUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    clock_t start, end, total_start, total_end;
    double pub_time, sub_time, total_time;
    long depth = 500000;
    long size = 1000;
    bool failure = false;

    /* Parse Inputs */
    if(argc == 2)
    {
        const char* depth_str = argv[0];
        const char* size_str = argv[1];
        if(StringLib::str2long(depth_str, &depth) == false)
        {
            mlog(CRITICAL, "[%d] ERROR: unable to parse depth\n", __LINE__);
            return -1;
        }
        else if(StringLib::str2long(size_str, &size) == false)
        {
            mlog(CRITICAL, "[%d] ERROR: unable to parse size\n", __LINE__);
            return -1;
        }
    }
    else if(argc != 0)
    {
        mlog(CRITICAL, "Invalid number of parameters passed to command: %d\n", argc);
        return -1;
    }

    /* Create Performance Test Data Structures */
    Publisher* p = new Publisher("testq_03", NULL);
    unsigned long sequence = 0;

    /* Iterate Over Number of Subscribers */
    mlog(INFO, "Depth, Size, Subscribers, Publishing, Subscribing, Total\n");
    for(int numsubs = 1; numsubs < MAX_SUBSCRIBERS; numsubs++)
    {
        perf_thread_t* info = new perf_thread_t[numsubs];

        total_start = clock();

        /* Kick-off Subscribers */
        Thread** t = new Thread* [numsubs];
        for(int i = 0; i < numsubs; i++)
        {
            info[i].s = new Subscriber("testq_03");
            info[i].v = new Sem();
            info[i].f = false;
            info[i].depth = depth;
            info[i].size = size;
            t[i] = new Thread(performanceThread, &info[i]);
        }

        /* Publish Packets */
        start = clock();
        unsigned char* pkt = new unsigned char [size];
        for(int i = 0; i < depth; i++)
        {
            for(int j = 0; j < size; j++)
            {
                pkt[j] = (unsigned char)sequence++;
            }

            int status = p->postCopy(pkt, size);
            if(status <= 0)
            {
                mlog(RAW, "[%d] ERROR: unable to post pkt %d with error %d\n", __LINE__, i, status);
                failure = true;
            }
        }
        delete [] pkt;
        end = clock();
        pub_time = ((double) (end - start)) / CLOCKS_PER_SEC;

        /* Start Subscribers */
        start = clock();
        for(int i = 0; i < numsubs; i++)
        {
            info[i].v->give();
        }

        /* Join Subscribers */
        for(int i = 0; i < numsubs; i++)
        {
            delete t[i]; // performs a join
            failure = failure || info[i].f; // checks status
        }
        delete [] t;
        end = clock();
        sub_time = ((double) (end - start)) / CLOCKS_PER_SEC;

        total_end = clock();
        total_time = ((double) (total_end - total_start)) / CLOCKS_PER_SEC;

        /* Print Results */
        mlog(INFO, "%ld, %ld, %d, %lf, %lf, %lf\n", depth, size, numsubs, pub_time, sub_time, total_time);

        /* Delete Info */
        for(int i = 0; i < numsubs; i++)
        {
            delete info[i].s;
            delete info[i].v;
        }
        delete [] info;
    }

    /* Delete Test Structures */
    delete p;

    if(failure) return -1;
    else        return 0;
}

/*----------------------------------------------------------------------------
 * subscriberOfOpporunityUnitTestCmd  -
 *----------------------------------------------------------------------------*/
int UT_MsgQ::subscriberOfOpporunityUnitTestCmd (int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;
    (void)argv;

    bool test_status = true;

    /* Set Unit Test Parameters (TODO: set from command parameters) */
    parms_t unit_test_parms;
    LocalLib::set(&unit_test_parms, 0, sizeof(unit_test_parms));
    unit_test_parms.qname = "testq_04";
    unit_test_parms.loopcnt = 5000;
    unit_test_parms.qdepth = 5000;
    unit_test_parms.numpubs = 10;
    unit_test_parms.numsubs = 10;

    /* Initialize Random Seed */
    srand((unsigned int)time(NULL));

    /* Create Thread Data */
    Thread** p_pid = new Thread* [unit_test_parms.numpubs];
    Thread** s_pid = new Thread* [unit_test_parms.numsubs];
    parms_t** pubparms = new parms_t* [unit_test_parms.numpubs];
    parms_t** subparms = new parms_t* [unit_test_parms.numsubs];

    /* Create Threads */
    for(int p = 0; p < unit_test_parms.numpubs; p++)
    {
        pubparms[p] = new parms_t;
        LocalLib::copy(pubparms[p], &unit_test_parms, sizeof(parms_t));
        pubparms[p]->threadid = p;
        p_pid[p] = new Thread(publisherThread, (void*)pubparms[p]);
    }
    for(int s = 0; s < unit_test_parms.numsubs; s++)
    {
        subparms[s] = new parms_t;
        LocalLib::copy(subparms[s], &unit_test_parms, sizeof(parms_t));
        subparms[s]->threadid = s;
        s_pid[s] = new Thread(opportunityThread, (void*)subparms[s]);
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
            mlog(RAW, "[%d] ERROR: SUB %d error count is %d\n", __LINE__, s, subparms[s]->errorcnt);
        }
        delete [] subparms[s]->lastvalue;
        delete subparms[s];
    }

    /* Check Results */
    int numq = MsgQ::numQ();
    if(numq > 0)
    {
        MsgQ::queueDisplay_t* msgQs = new MsgQ::queueDisplay_t[numq];
        int cnumq = MsgQ::listQ(msgQs, numq);
        for(int i = 0; i < cnumq; i++)
        {
            if(StringLib::match(msgQs[i].name, unit_test_parms.qname))
            {
                if(msgQs[i].subscriptions != 0)
                {
                    test_status = false;
                    mlog(RAW, "[%d] ERROR: msgQ %40s %8d %9s %d failed to unsubscribe all subscribers\n", __LINE__, msgQs[i].name, msgQs[i].len, msgQs[i].state, msgQs[i].subscriptions);
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
    if(test_status) return 0;
    else            return -1;
}

/*----------------------------------------------------------------------------
 * subscriberThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::subscriberThread(void* parm)
{
    parms_t* unit_test_parms = (parms_t*)parm;

    /* Initialize Checking Variables */
    unit_test_parms->lastvalue = new long[unit_test_parms->numpubs];
    bool* first_read = new bool[unit_test_parms->numpubs];
    for(int p = 0; p < unit_test_parms->numpubs; p++) first_read[p] = true;

    /* Create Queue */
    randomDelay(100);
    Subscriber* q = new Subscriber(unit_test_parms->qname, MsgQ::SUBSCRIBER_OF_CONFIDENCE, unit_test_parms->qdepth);
    mlog(RAW, "Subscriber thread %d created on queue %s\n", unit_test_parms->threadid, unit_test_parms->qname);

    /* Loop */
    long data;
    int loops = unit_test_parms->loopcnt * unit_test_parms->numpubs;
    while(loops--)
    {
        randomDelay(1);
        int status = q->receiveCopy((void*)&data, sizeof(long), 1000 * unit_test_parms->numpubs);
        if(status > 0)
        {
            int threadid = data >> 16;
            if(threadid < 0 || threadid >= unit_test_parms->numpubs)
            {
                mlog(RAW, "[%d] ERROR: out of bounds threadid in %d: %d", __LINE__, unit_test_parms->threadid, threadid);
                unit_test_parms->errorcnt++;
                break;
            }
            else if(first_read[threadid])
            {
                first_read[threadid] = false;
            }
            else if(data != unit_test_parms->lastvalue[threadid] + 1)
            {
                mlog(RAW, "[%d] ERROR: read %d sequence error %ld != %ld + 1\n", __LINE__, unit_test_parms->threadid, data, unit_test_parms->lastvalue[threadid]);
                unit_test_parms->errorcnt++;
            }
            unit_test_parms->lastvalue[threadid] = data;
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            mlog(RAW, "Subscriber thread %d encountered timeout\n", unit_test_parms->threadid);
            break;
        }
        else
        {
            mlog(RAW, "[%d] ERROR: %d error %d\n", __LINE__, unit_test_parms->threadid, status);
            unit_test_parms->errorcnt++;
            break;
        }
    }

    mlog(RAW, "Subscriber thread %d exited with %d loops to go\n", unit_test_parms->threadid, loops + 1);
    
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
    parms_t* unit_test_parms = (parms_t*)parm;

    /* Last Value Instantiation */
    unit_test_parms->lastvalue = new long;

    /* Create Queue */
    randomDelay(100);
    Publisher* q = new Publisher(unit_test_parms->qname, NULL, unit_test_parms->qdepth);
    mlog(RAW, "Publisher thread %d created on queue %s\n", unit_test_parms->threadid, unit_test_parms->qname);

    /* Loop */
    int timeout_cnt = 0;
    long data = (unit_test_parms->threadid << 16) + 1;
    int loops = unit_test_parms->loopcnt;
    while(loops--)
    {
        randomDelay(1);
        int status = q->postCopy((void*)&data, sizeof(long), 2000 * unit_test_parms->numpubs);
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
            mlog(RAW, "[%d] ERROR: post %d error %d\n", __LINE__, unit_test_parms->threadid, status);
            unit_test_parms->errorcnt++;
            break;
        }
    }

    mlog(RAW, "Publisher thread %d encountered %d timeouts at data %ld\n", unit_test_parms->threadid, timeout_cnt, data & 0xFFFF);
    
    /* Clean Up */
    delete q;

    return NULL;
}

/*----------------------------------------------------------------------------
 * performanceThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::performanceThread(void* parm)
{
    perf_thread_t* info = (perf_thread_t*)parm;
    unsigned long sequence = 0;

    /* Wait to Start */
    info->v->take();

    /* Drain Queue */
    for(int pktnum = 0; pktnum < info->depth; pktnum++)
    {
        Subscriber::msgRef_t ref;
        int status = info->s->receiveRef(ref, SYS_TIMEOUT);
        if(status > 0)
        {
            if(ref.size != info->size)
            {
                mlog(RAW, "[%d] ERROR:  mismatched size of receive: %d != %d\n", __LINE__, ref.size, info->size);
                info->f = true;
            }
            else
            {
                unsigned char* pkt = (unsigned char*)ref.data;
                for(int i = 0; i < info->size; i++)
                {
                    if(pkt[i] != (unsigned char)sequence++)
                    {
                        mlog(RAW, "[%d] ERROR:  invalid sequence detected in data: %d != %d\n", __LINE__, pkt[i], (unsigned char)(sequence - 1));
                        info->f = true;
                    }
                }
            }

            info->s->dereference(ref);
        }
        else if(status == MsgQ::STATE_TIMEOUT)
        {
            mlog(RAW, "[%d] ERROR:  unexpected timeout on receive at pkt %d!\n", __LINE__, pktnum);
            info->f = true;
        }
        else
        {
            mlog(RAW, "[%d] ERROR:  failed to receive message, error %d\n", __LINE__, status);
            info->f = true;
        }
    }

    /* Check Empty */
    Subscriber::msgRef_t ref;
    int status = info->s->receiveRef(ref, IO_CHECK);
    if(status != MsgQ::STATE_EMPTY)
    {
        mlog(RAW, "[%d] ERROR: queue unexpectedly not empty, return status %d\n", __LINE__, status);
        info->f = true;
    }

    /* Clean up and exit */
    return NULL;
}

/*----------------------------------------------------------------------------
 * opportunityThread  -
 *----------------------------------------------------------------------------*/
void* UT_MsgQ::opportunityThread(void* parm)
{
    parms_t* unit_test_parms = (parms_t*)parm;

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
        int status = q->receiveCopy((void*)&data, sizeof(long), SYS_TIMEOUT);
        if(status > 0)
        {
            int threadid = data >> 16;
            if(threadid < 0 || threadid >= unit_test_parms->numpubs)
            {
                mlog(RAW, "[%d] ERROR: out of bounds threadid in %d: %d\n", __LINE__, unit_test_parms->threadid, threadid);
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
            mlog(RAW, "[%d] ERROR: %d error %d\n", __LINE__, unit_test_parms->threadid, status);
            unit_test_parms->errorcnt++;
            break;
        }
        else if(timeouts++ > 1)
        {
            break; // test over
        }
    }

    /* Display Drop Count */
    mlog(INFO, "Exiting subscriber of opportunity %d test loop at count %d with %d drops\n", unit_test_parms->threadid, loops, drops);

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
    long us = rand() % (max_milliseconds * 1000);

#ifdef _LINUX_
    struct timespec waittime;
    waittime.tv_sec  = us / 1000000;
    waittime.tv_nsec = (us % 1000000) * 1000;
    while( nanosleep(&waittime, &waittime) == -1 ) continue;
#else 
#ifdef _WINDOWS_
    Sleep(us / 1000);
#endif
#endif
}
