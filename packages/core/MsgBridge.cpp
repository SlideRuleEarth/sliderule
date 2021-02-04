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

#include "MsgBridge.h"
#include "core.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* MsgBridge::LuaMetaName = "MsgBridge";
const struct luaL_Reg MsgBridge::LuaMetaTable[] = {
    {NULL,          NULL}
};

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - bridge(<input stream name>, <output stream name>)
 *----------------------------------------------------------------------------*/
int MsgBridge::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        const char* inq_name    = getLuaString(L, 1);
        const char* outq_name   = getLuaString(L, 2);

        /* Create Record bridge */
        return createLuaObject(L, new MsgBridge(L, inq_name, outq_name));
    }
    catch(const RunTimeException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.what());
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
MsgBridge::MsgBridge(lua_State* L, const char* inputq_name, const char* outputq_name):
    LuaObject(L, BASE_OBJECT_TYPE, LuaMetaName, LuaMetaTable)
{
    assert(inputq_name);
    assert(outputq_name);

    /* Create Bridge MsgQs */
    inQ = new Subscriber(inputq_name);
    outQ = new Publisher(outputq_name);

    /* Create Thread Pool */
    active = true;
    pid = new Thread(bridgeThread, this);
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
MsgBridge::~MsgBridge(void)
{
    active = false;
    delete pid;
    delete inQ;
    delete outQ;
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * bridgeThread
 *----------------------------------------------------------------------------*/
void* MsgBridge::bridgeThread(void* parm)
{
    MsgBridge* bridge = (MsgBridge*)parm;

    /* Loop Forever */
    while(bridge->active)
    {
        /* Receive Message */
        Subscriber::msgRef_t ref;
        int recv_status = bridge->inQ->receiveRef(ref, SYS_TIMEOUT);
        if(recv_status > 0)
        {
            unsigned char* msg = (unsigned char*)ref.data;
            int len = ref.size;

            /* Dispatch Record */
            if(len > 0)
            {
                int status = MsgQ::STATE_TIMEOUT;
                while(bridge->active && status == MsgQ::STATE_TIMEOUT)
                {
                    status = bridge->outQ->postCopy(msg, len, SYS_TIMEOUT);
                    if(status < 0)
                    {
                        mlog(CRITICAL, "Failed (%d) bridge from %s to %s... exiting!\n", status, bridge->inQ->getName(), bridge->outQ->getName());
                        bridge->active = false;
                    }
                }
            }
            else
            {
                /* Terminating Message */
                mlog(INFO, "Terminator received on %s, exiting bridge\n", bridge->inQ->getName());
                bridge->active = false; // breaks out of loop
            }

            /* Dereference Message */
            bridge->inQ->dereference(ref);
        }
        else if(recv_status != MsgQ::STATE_TIMEOUT)
        {
            /* Break Out on Failure */
            mlog(CRITICAL, "Failed queue receive on %s with error %d\n", bridge->inQ->getName(), recv_status);
            bridge->active = false; // breaks out of loop
        }
    }

    /* Signal Completion */
    bridge->signalComplete();

    return NULL;
}
