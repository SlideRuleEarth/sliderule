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

#include "DeviceWriter.h"
#include "DeviceIO.h"
#include "LogLib.h"
#include "OsApi.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - writer(<device>, <output stream name>)
 *----------------------------------------------------------------------------*/
int DeviceWriter::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        DeviceObject*   _device = (DeviceObject*)getLuaObject(L, 1, DeviceObject::OBJECT_TYPE);
        const char*     q_name  = getLuaString(L, 2, true, NULL);

        /* Return DeviceReader Object */
        return createLuaObject(L, new DeviceWriter(L, _device, q_name));
    }
    catch(const LuaException& e)
    {
        mlog(CRITICAL, "Error creating %s: %s\n", LuaMetaName, e.errmsg);
        return returnLuaStatus(L, false);
    }
}

/******************************************************************************
 * PRIVATE METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor
 *----------------------------------------------------------------------------*/
DeviceWriter::DeviceWriter(lua_State* L, DeviceObject* _device, const char* inq_name):
    DeviceIO(L, _device)
{
    inq = NULL;
    if(inq_name)
    {
        inq = new Subscriber(inq_name);
        ioActive = true;
        ioThread = new Thread(writerThread, this);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
DeviceWriter::~DeviceWriter(void)
{
    /* Prevent Double Death */
    dieOnDisconnect = false;

    /* Stop Thread */
    ioActive = false;
    if(ioThread) delete ioThread;

    /* Delete Input Stream */
    if(inq) delete inq;

    /* Unlock */
    device->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * writerThread
 *----------------------------------------------------------------------------*/
void* DeviceWriter::writerThread (void* parm)
{
    assert(parm != NULL);
    DeviceWriter* dw = (DeviceWriter*)parm;

    /* Read Loop */
    while(dw->ioActive)
    {
        /* Read Bytes */
        Subscriber::msgRef_t ref;
        int status = dw->inq->receiveRef(ref, dw->blockCfg);

        /* Process Bytes */
        if(status > 0)
        {
            /* Send Message */
            int bytes_sent = 0;
            while(bytes_sent <= 0 && dw->ioActive)
            {
                bytes_sent = dw->device->writeBuffer(ref.data, ref.size);
                if(bytes_sent > 0)
                {
                    dw->bytesProcessed += bytes_sent;
                    dw->packetsProcessed += 1;
                }
                else if(bytes_sent != TIMEOUT_RC)
                {
                    dw->bytesDropped += ref.size;
                    dw->packetsDropped += 1;
                    mlog(ERROR, "Failed (%d) to write to device with error: %s\n", bytes_sent, LocalLib::err2str(errno));

                    /* Handle Non-Timeout Errors */
                    if(dw->dieOnDisconnect)
                    {
                        mlog(CRITICAL, "... closing connection and exiting writer!\n");
                        dw->ioActive = false; // breaks out of loop
                    }
                    else
                    {
                        mlog(ERROR, "failed to write to device with error... sleeping and going on to next message!\n");
                        LocalLib::sleep(1); // prevent spin
                        break; // stop trying to send this message and go to the next one
                    }
                }
            }

            /* Dereference Message */
            dw->inq->dereference(ref);
        }
        else if(status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "encountered a fatal error (%d) reading from input stream %s, exiting writer!\n", status, dw->inq->getName());
            dw->ioActive = false; // breaks out of loop
        }
        else // TIMEOUT
        {
            // let device handle timeout
            dw->device->writeBuffer(NULL, 0);
        }
    }

    /* Clean Up */
    dw->device->closeConnection();
    return NULL;
}
