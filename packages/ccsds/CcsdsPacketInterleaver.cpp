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

#include "CcsdsPacketInterleaver.h"
#include "CcsdsPacket.h"
#include "core.h"
#include <float.h>

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsPacketInterleaver::OBJECT_TYPE = "CcsdsPacketInterleaver";
const char* CcsdsPacketInterleaver::LuaMetaName = "CcsdsPacketInterleaver";
const struct luaL_Reg CcsdsPacketInterleaver::LuaMetaTable[] = {
    {"start",       luaSetStartTime},
    {"stop",        luaSetStopTime},
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - interleave([<inq1, inq2, ...>], <outq>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketInterleaver::luaCreate (lua_State* L)
{
    try
    {        
        /* Get Input Queues */
        MgList<const char*,true> inq_names;
        int inq_table_index = 1;
        if(lua_istable(L, inq_table_index))
        {
            /* Get number of names in table */
            int num_names = lua_rawlen(L, inq_table_index);

            /* Iterate through each name in table */
            for(int i = 0; i < num_names; i++)
            {
                /* Get name */
                lua_rawgeti(L, inq_table_index, i+1);
                const char* name_str = StringLib::duplicate(getLuaString(L, -1));

                /* Add name to list */
                inq_names.add(name_str);

                /* Clean up stack */
                lua_pop(L, 1);
            }
        }

        /* Get Output Queue */
        const char* outq_name = getLuaString(L, 2);

        /* Create Lua Object */
        return createLuaObject(L, new CcsdsPacketInterleaver(L, inq_names, outq_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating CcsdsPacketInterleaver: %s\n", e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
CcsdsPacketInterleaver::CcsdsPacketInterleaver(lua_State* L, MgList<const char*,true>& inq_names, const char* outq_name):
    LuaObject(L, OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    /* Create Input Streams */
    for(int i = 0; i < inq_names.length(); i++)
    {
       Subscriber* sub = new Subscriber(inq_names[i]);
       inQs.add(sub);
    }

    /* Create Output Stream */
    outQ = new Publisher(outq_name);

    /* Initialize Times */
    startTime = 0;
    stopTime = 0;

    /* Create Thread */
    active = true;
    pid = new Thread(processorThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
CcsdsPacketInterleaver::~CcsdsPacketInterleaver(void)
{
    active = false;
    delete pid;
    for(int i = 0; i < inQs.length(); i++)
    {
        delete inQs[i];
    }
    delete outQ;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processorThread
 *----------------------------------------------------------------------------*/
void* CcsdsPacketInterleaver::processorThread(void* parm)
{
    assert(parm);

    CcsdsPacketInterleaver* processor = (CcsdsPacketInterleaver*)parm;

    /* Get Number of Inputs */
    int num_inputs = processor->inQs.length();
    if(num_inputs <= 0)
    {
        mlog(CRITICAL, "Must have at least one input\n");
        return NULL;
    }

    /* Create Packet Arrays */
    double* pkt_times = new double[num_inputs];
    Subscriber::msgRef_t* pkt_refs = new Subscriber::msgRef_t[num_inputs];
    bool* inq_valid = new bool[num_inputs];

    /* Initialize Packet Arrays */
    for(int i = 0; i < num_inputs; i++)
    {
        pkt_refs[i].size = 0;
        inq_valid[i] = true;
    }

    /* Loop While Read Active */
    int num_valid = num_inputs;
    while(processor->active && num_valid > 0)
    {
        /* Read Packets */
        for(int i = 0; i < num_inputs; i++)
        {
            if(inq_valid[i] && pkt_refs[i].size == 0)
            {
                int status = processor->inQs[i]->receiveRef(pkt_refs[i], SYS_TIMEOUT);
                if(status > 0)
                {
                    if(pkt_refs[i].size > 0)
                    {
                        /* Capture Packet Time */
                        CcsdsSpacePacket pkt((unsigned char*)pkt_refs[i].data, pkt_refs[i].size);
                        pkt_times[i] = pkt.getCdsTime();

                        /* Check Time Filter */
                        if(processor->startTime > 0 && pkt_times[i] < processor->startTime)
                        {
                            processor->inQs[i]->dereference(pkt_refs[i]);
                            pkt_refs[i].size = 0;
                        }
                        else if(processor->stopTime > 0 && pkt_times[i] > processor->stopTime)
                        {
                            processor->inQs[i]->dereference(pkt_refs[i]);
                            pkt_refs[i].size = 0;
                        }
                    }
                    else
                    {
                        /* Terminator Received */
                        processor->inQs[i]->dereference(pkt_refs[i]);
                        inq_valid[i] = false;
                        num_valid--;
                        mlog(CRITICAL, "Terminator received on %s (%d remaining)\n", processor->inQs[i]->getName(), num_valid);
                    }
                }
                else if(status != MsgQ::STATE_TIMEOUT)
                {
                    mlog(CRITICAL, "Failed to read from input queue %s: %d\n", processor->inQs[i]->getName(), status);
                    inq_valid[i] = false;
                    num_valid--;
                }
            }
        }

        /* Find Packet To Send */
        int earliest_pkt = -1;
        double earliest_pkt_time = DBL_MAX;
        for(int i = 0; i < num_inputs; i++)
        {
            if(inq_valid[i] && pkt_refs[i].size > 0)
            {
                if(pkt_times[i] < earliest_pkt_time)
                {
                    earliest_pkt = i;
                    earliest_pkt_time = pkt_times[i];
                }
            }
        }

        /* Send Earliest Packet */
        if(earliest_pkt >= 0)
        {
            int status = MsgQ::STATE_TIMEOUT;
            while(processor->active && status == MsgQ::STATE_TIMEOUT)
            {
                status = processor->outQ->postCopy(pkt_refs[earliest_pkt].data, pkt_refs[earliest_pkt].size, SYS_TIMEOUT);
                if(status > 0)
                {
                    processor->inQs[earliest_pkt]->dereference(pkt_refs[earliest_pkt]);
                    pkt_refs[earliest_pkt].size = 0;
                }
                else if(status == MsgQ::STATE_TIMEOUT)
                {
                    printf("WHY ARE WE TIMING OUT\n");
                }
                else
                {
                    mlog(CRITICAL, "Failed to post to %s... exiting interleaver!\n", processor->outQ->getName());
                    processor->active = false;
                }
            }
        }
    }

    /* Dereference Outstanding Messages */
    for(int i = 0; i < num_inputs; i++)
    {
        if(inq_valid[i] && pkt_refs[i].size > 0)
        {
            processor->inQs[i]->dereference(pkt_refs[i]);
        }
    }

    /* Free Packet Arrays */
    delete [] pkt_times;
    delete [] pkt_refs;
    delete [] inq_valid;

    /* Signal Complete */
    processor->signalComplete();
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaSetStartTime - :start(<gmt time>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketInterleaver::luaSetStartTime (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketInterleaver* lua_obj = (CcsdsPacketInterleaver*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* gmt_str = getLuaString(L, 2);

        /* Get Time */
        int64_t gmt_ms = TimeLib::str2gpstime(gmt_str);
        if(gmt_ms == 0)
        {
            throw LuaException("failed to parse time string %s", gmt_str);
        }

        /* Set Start Time */
        lua_obj->startTime = (double)gmt_ms / 1000.0;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting start time: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}

/*----------------------------------------------------------------------------
 * luaSetStopTime - :stop(<gmt time>)
 *----------------------------------------------------------------------------*/
int CcsdsPacketInterleaver::luaSetStopTime (lua_State* L)
{
    bool status = false;

    try
    {
        /* Get Self */
        CcsdsPacketInterleaver* lua_obj = (CcsdsPacketInterleaver*)getLuaSelf(L, 1);

        /* Get Parameters */
        const char* gmt_str = getLuaString(L, 2);

        /* Get Time */
        int64_t gmt_ms = TimeLib::str2gpstime(gmt_str);
        if(gmt_ms == 0)
        {
            throw LuaException("failed to parse time string %s", gmt_str);
        }

        /* Set Stop Time */
        lua_obj->stopTime = (double)gmt_ms / 1000.0;
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error setting stop time: %s\n", e.errmsg);
    }

    /* Return Status */
    return returnLuaStatus(L, status);
}
