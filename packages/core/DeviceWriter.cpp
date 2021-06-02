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

#include "DeviceWriter.h"
#include "DeviceIO.h"
#include "EventLib.h"
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
    catch(const RunTimeException& e)
    {
        mlog(e.level(), "Error creating %s: %s", LuaMetaName, e.what());
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

    /* Release Device */
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
            if(ref.size > 0)
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
                        mlog(ERROR, "Failed (%d) to write to device with error: %s", bytes_sent, LocalLib::err2str(errno));

                        /* Handle Non-Timeout Errors */
                        if(dw->dieOnDisconnect)
                        {
                            mlog(CRITICAL, "... closing connection and exiting writer!");
                            dw->ioActive = false; // breaks out of loop
                        }
                        else
                        {
                            mlog(ERROR, "failed to write to device with error... sleeping and going on to next message!");
                            LocalLib::sleep(1); // prevent spin
                            break; // stop trying to send this message and go to the next one
                        }
                    }
                }
            }
            else
            {
                /* Received Terminating Message */
                mlog(INFO, "Terminator received on %s, exiting device writer", dw->inq->getName());
                dw->ioActive = false; // breaks out of loop
            }

            /* Dereference Message */
            dw->inq->dereference(ref);
        }
        else if(status != MsgQ::STATE_TIMEOUT)
        {
            mlog(CRITICAL, "encountered a fatal error (%d) reading from input stream %s, exiting writer!", status, dw->inq->getName());
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
    dw->signalComplete();
    return NULL;
}
