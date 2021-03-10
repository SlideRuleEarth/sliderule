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
