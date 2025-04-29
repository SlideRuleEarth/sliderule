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

#include "Monitor.h"
#include "EventLib.h"
#include "TimeLib.h"
#include "RecordObject.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* Monitor::OBJECT_TYPE = "Monitor";
const char* Monitor::LUA_META_NAME = "Monitor";
const struct luaL_Reg Monitor::LUA_META_TABLE[] = {
    {"config",      luaConfig},
    {NULL,          NULL}
};

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
Monitor::Monitor(lua_State* L, event_level_t level, const char* eventq_name, const char* rec_type):
    LuaObject(L, OBJECT_TYPE, LUA_META_NAME, LUA_META_TABLE),
    eventLevel(level)
{
    /* Initialize Event Monitor Thread*/
    active = true;
    inQ = new Subscriber(eventq_name);
    pid = new Thread(monitorThread, this);
    recType = StringLib::duplicate(rec_type);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
Monitor::~Monitor(void)
{
    active = false;
    delete pid;
    delete inQ;
    delete [] recType;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * processEvent
 *----------------------------------------------------------------------------*/
void* Monitor::monitorThread (void* parm)
{
    Monitor* monitor = static_cast<Monitor*>(parm);

    /* Loop Forever */
    while(monitor->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        const int recv_status = monitor->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            unsigned char* msg = reinterpret_cast<unsigned char*>(ref.data);
            const int len = ref.size;
            if(len > 0)
            {
                try
                {
                    const RecordInterface record(msg, len);
                    if(StringLib::match(record.getRecordType(), monitor->recType))
                    {
                        /* Process Event Data */
                        unsigned char* event_data = record.getRecordData();
                        const int event_size = record.getAllocatedDataSize();
                        monitor->processEvent(event_data, event_size);
                    }
                }
                catch (const RunTimeException& e)
                {
                    /*
                     * Can only print to terminal here because a log
                     * message will recursively re-enter this function
                     */
                    print2term("Error processing event: %s", e.what());
                }
            }
            else
            {
                /* Terminating Message */
                mlog(DEBUG, "Terminator received on %s, exiting monitor", monitor->inQ->getName());
                monitor->active = false; // breaks out of loop
            }

            /* Dereference Message */
            monitor->inQ->dereference(ref);
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive on %s with error %d", monitor->inQ->getName(), recv_status);
            monitor->active = false; // breaks out of loop
        }
    }

    /* Signal Completion */
    monitor->signalComplete();
    return NULL;
}

/*----------------------------------------------------------------------------
 * luaConfig - :config([<level>]) --> level, status
 *----------------------------------------------------------------------------*/
int Monitor::luaConfig (lua_State* L)
{
    bool status = false;
    int num_ret = 1;
    bool provided = false;

    try
    {
        /* Get Self */
        Monitor* lua_obj = dynamic_cast<Monitor*>(getLuaSelf(L, 1));

        /* Set Level */
        const event_level_t level = (event_level_t)getLuaInteger(L, 1, true, static_cast<long>(CRITICAL), &provided);
        if(provided) lua_obj->eventLevel = level;

        /* Set Return Values */
        lua_pushinteger(L, (int)lua_obj->eventLevel);
        num_ret++;

        /* Set return Status */
        status = true;
    }
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error configuring monitor: %s", e.what());
    }

    /* Return Status */
    return returnLuaStatus(L, status, num_ret);
}
