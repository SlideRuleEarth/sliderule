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

#include "DeviceReader.h"
#include "DeviceIO.h"
#include "EventLib.h"
#include "MsgQ.h"

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * luaCreate - reader(<device>, <output stream name>)
 *----------------------------------------------------------------------------*/
int DeviceReader::luaCreate (lua_State* L)
{
    try
    {
        /* Get Parameters */
        DeviceObject*   _device = (DeviceObject*)getLuaObject(L, 1, DeviceObject::OBJECT_TYPE);
        const char*     q_name  = getLuaString(L, 2, true, NULL);

        /* Return DeviceReader Object */
        return createLuaObject(L, new DeviceReader(L, _device, q_name));
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
DeviceReader::DeviceReader(lua_State* L, DeviceObject* _device, const char* outq_name):
    DeviceIO(L, _device)
{
    outq = NULL;
    if(outq_name)
    {
        outq = new Publisher(outq_name);
        ioActive = true;
        ioThread = new Thread(readerThread, this);
    }
}

/*----------------------------------------------------------------------------
 * Destructor
 *----------------------------------------------------------------------------*/
DeviceReader::~DeviceReader(void)
{
    /* Prevent Double Death */
    dieOnDisconnect = false;

    /* Stop Thread */
    ioActive = false;
    if(ioThread) delete ioThread;

    /* Delete Output Queue */
    if(outq) delete outq;

    /* Release Device */
    device->releaseLuaObject();
}

/*----------------------------------------------------------------------------
 * readerThread
 *----------------------------------------------------------------------------*/
void* DeviceReader::readerThread (void* parm)
{
    assert(parm != NULL);
    DeviceReader* dr = (DeviceReader*)parm;
    int io_maxsize = LocalLib::getIOMaxsize();
    unsigned char* buf = new unsigned char [io_maxsize];

    /* Read Loop */
    while(dr->ioActive)
    {
        /* Read Device */
        int bytes = dr->device->readBuffer(buf, io_maxsize);
        if(bytes > 0)
        {
            /* Post Message */
            int post_status = MsgQ::STATE_ERROR;
            while(dr->ioActive && (post_status = dr->outq->postCopy(buf, bytes, dr->blockCfg)) <= 0)
            {
                mlog(ERROR, "Device reader unable to post to stream %s: %d", dr->outq->getName(), post_status);
            }

            /* Update Statistics */
            if(post_status > 0)
            {
                dr->bytesProcessed += bytes;
                dr->packetsProcessed += 1;
            }
            else
            {
                dr->bytesDropped += bytes;
                dr->packetsDropped += 1;
            }
        }
        else if(bytes != TIMEOUT_RC)
        {
            /* Handle Non-Timeout Errors */
            if(dr->dieOnDisconnect)
            {
                if(bytes == SHUTDOWN_RC)    mlog(INFO, "shutting down device and exiting reader");
                else                        mlog(CRITICAL, "failed to read device (%d)... closing connection and exiting reader!", bytes);
                dr->ioActive = false; // breaks out of loop
            }
            else
            {
                if(bytes == SHUTDOWN_RC)    mlog(INFO, "shutting down device... sleeping and trying again");
                else                        mlog(ERROR, "failed to read device (%d)... sleeping and trying again!", bytes);
                LocalLib::performIOTimeout(); // prevent spinning
            }
        }
    }

    /* Clean Up */
    delete [] buf;
    dr->device->closeConnection();
    dr->signalComplete();
    dr->outq->postCopy("", 0); // send terminator
    return NULL;
}
