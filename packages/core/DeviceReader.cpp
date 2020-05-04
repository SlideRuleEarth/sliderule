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

#include "DeviceReader.h"
#include "DeviceIO.h"
#include "LogLib.h"
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
        DeviceObject*   _device = (DeviceObject*)lockLuaObject(L, 1, DeviceObject::OBJECT_TYPE);
        const char*     q_name  = getLuaString(L, 2, true, NULL);

        /* Return DeviceReader Object */
        return createLuaObject(L, new DeviceReader(L, _device, q_name));
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

    /* Unlock */
    device->removeLock();
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
                mlog(ERROR, "Device reader unable to post to stream %s: %d\n", dr->outq->getName(), post_status);
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
                if(bytes == SHUTDOWN_RC)    mlog(CRITICAL, "shutting down device and exiting reader\n");
                else                        mlog(CRITICAL, "failed to read device (%d)... closing connection and exiting reader!\n", bytes);
                dr->ioActive = false; // breaks out of loop
            }
            else
            {
                if(bytes == SHUTDOWN_RC)    mlog(CRITICAL, "shutting down device... sleeping and trying again\n");
                else                        mlog(ERROR, "failed to read device (%d)... sleeping and trying again!\n", bytes);
                LocalLib::performIOTimeout(); // prevent spinning
            }
        }
    }

    /* Clean Up */
    delete [] buf;
    dr->device->closeConnection();
    return NULL;
}
