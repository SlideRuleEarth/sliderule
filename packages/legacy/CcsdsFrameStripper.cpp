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

#include "CcsdsFrameStripper.h"
#include "core.h"
#include "ccsds.h"

/******************************************************************************
 * STATIC DATA
 ******************************************************************************/

const char* CcsdsFrameStripper::TYPE = "CcsdsFrameStripper";

/******************************************************************************
 * PUBLIC METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * createObject  -
 *
 *   Notes: Command Processor Command
 *----------------------------------------------------------------------------*/
CommandableObject* CcsdsFrameStripper::createObject(CommandProcessor* cmd_proc, const char* name, int argc, char argv[][MAX_CMD_SIZE])
{
    (void)argc;

    const char* inq_name = StringLib::checkNullStr(argv[0]);
    const char* outq_name = StringLib::checkNullStr(argv[1]);
    const char* sync_str = argv[2];
    const char* strip_str = argv[3];
    const char* fixed_str = argv[4];

    if(inq_name == NULL)
    {
        mlog(CRITICAL, "Must supply a input queue name\n");
        return NULL;
    }

    if(outq_name == NULL)
    {
        mlog(CRITICAL, "Must supply a output queue name\n");
        return NULL;
    }

    int sync_size = 0;
    uint8_t sync_marker[MAX_STR_SIZE];
    if(!StringLib::match(sync_str, "NONE"))
    {
        sync_size = (int)StringLib::size(sync_str);
        if(sync_size <= 0 || sync_size % 2 != 0)
        {
            mlog(CRITICAL, "Sync marker is an invalid length: %d\n", sync_size);
            return NULL;
        }
        else if(sync_size > MAX_STR_SIZE)
        {
            mlog(CRITICAL, "Sync marker is too long: %d\n", sync_size);
            return NULL;
        }
        sync_size /= 2;

        char numstr[5] = {'0', 'x', '\0', '\0', '\0'};
        for(int i = 0; i < (sync_size * 2); i+=2)
        {
            numstr[2] = sync_str[i];
            numstr[3] = sync_str[i+1];

            unsigned long val;
            if(!StringLib::str2ulong(numstr, &val))
            {
                mlog(CRITICAL, "Unable to parse sync marker at %d: %s\n", i, numstr);
                return NULL;
            }

            sync_marker[i / 2] = (uint8_t)val;
        }
    }

    unsigned long strip_size;
    if(!StringLib::str2ulong(strip_str, &strip_size))
    {
        mlog(CRITICAL, "Error: invalid strip size: %s\n", strip_str);
        return NULL;
    }

    long fixed_size;
    if(!StringLib::str2long(fixed_str, &fixed_size))
    {
        mlog(CRITICAL, "Unable to parse fixed frame size: %s\n", fixed_str);
        return NULL;
    }


    return new CcsdsFrameStripper(cmd_proc, name, inq_name, outq_name, sync_marker, sync_size, strip_size, fixed_size);
}

/******************************************************************************
 * PROTECTED METHODS
 ******************************************************************************/

/*----------------------------------------------------------------------------
 * Constructor  -
 *----------------------------------------------------------------------------*/
CcsdsFrameStripper::CcsdsFrameStripper(CommandProcessor* cmd_proc, const char* obj_name, const char* inq_name, const char* outq_name, uint8_t* sync_marker, int sync_size, int strip_size, int frame_size):
    CcsdsMsgProcessor(cmd_proc, obj_name, TYPE, inq_name)
{
    pubQ = new Publisher(outq_name);

    LStripSize      = strip_size;
    SyncMarkerSize  = sync_size;
    FrameFixedSize  = frame_size;
    SyncMarker      = NULL;
    if(SyncMarkerSize > 0)
    {
        SyncMarker = new uint8_t [sync_size];
        LocalLib::copy(SyncMarker, sync_marker, sync_size);
    }

    if(LStripSize > 0)          state = LSTRIP;
    else if(SyncMarkerSize > 0) state = SYNC;
    else                        state = FRAME;

    lStripBytes = LStripSize;
    syncIndex   = 0;
    frameIndex  = 0;

    inSync = false;

    frameBuffer = new uint8_t [FrameFixedSize];

    start(); // start processor
}

/*----------------------------------------------------------------------------
 * Destructor  -
 *----------------------------------------------------------------------------*/
CcsdsFrameStripper::~CcsdsFrameStripper(void)
{
    stop(); // stop processor

    delete pubQ;
    delete [] frameBuffer;
}

/*----------------------------------------------------------------------------
 * processMsg  - implementation of virtual method for handling messages
 *----------------------------------------------------------------------------*/
bool CcsdsFrameStripper::processMsg (unsigned char* msg, int bytes)
{

    unsigned char*  parse_buffer = msg;
    int             parse_bytes = bytes;
    int             parse_index = 0;

    /* Parse Buffer */
    while(parse_index < parse_bytes)
    {
        /* Determine Number of Bytes left */
        int bytes_left = parse_bytes - parse_index;

        if(state == LSTRIP)
        {
            /* Strip off leading bytes */
            if(lStripBytes <= bytes_left)
            {
                parse_index += lStripBytes;
                lStripBytes  = LStripSize;

                if(SyncMarkerSize > 0)  state = SYNC;
                else                    state = FRAME;
            }
            else
            {
                lStripBytes -= bytes_left;
                parse_index += bytes_left;
            }
        }
        else if(state == SYNC)
        {
            while(state == SYNC && parse_index < parse_bytes && syncIndex < SyncMarkerSize)
            {
                if(parse_buffer[parse_index] != SyncMarker[syncIndex])
                {
                    syncIndex = 0;
                    if(inSync)
                    {
                        mlog(CRITICAL, "Lost sync in processing frames in %s\n", getName());
                        inSync = false;
                    }
                }
                else
                {
                    syncIndex++;
                    if(syncIndex == SyncMarkerSize)
                    {
                        syncIndex = 0;
                        if(!inSync)
                        {
                            mlog(CRITICAL, "Synchronization of frames acquired in %s\n", getName());
                            inSync = true;
                        }

                        state = FRAME;
                    }
                }

                parse_index++;
            }
        }
        else if(state == FRAME)
        {
            /* Check that Bytes Left Fit in Frame */
            if((bytes_left + frameIndex) > FrameFixedSize)
            {
                bytes_left = FrameFixedSize - frameIndex;
            }

            /* Add Bytes Left to Frame Buffer */
            LocalLib::copy(&frameBuffer[frameIndex], &parse_buffer[parse_index], bytes_left);
            frameIndex += bytes_left;
            parse_index += bytes_left;
        }
        /* Error in State Machine - Bug in Code */
        else
        {
            assert(false);
        }

        /* Check for End of Frame */
        if(frameIndex >= FrameFixedSize)
        {
            /* Post Frame Buffer */
            int status = pubQ->postCopy(frameBuffer, FrameFixedSize, SYS_TIMEOUT);
            if(status <= 0)
            {
                mlog(CRITICAL, "Frame unable to be posted[%d] to output stream %s\n", status, pubQ->getName());
            }

            /* Goto Initial State */
            if(LStripSize > 0)          state = LSTRIP;
            else if(SyncMarkerSize > 0) state = SYNC;
            else                        state = FRAME;

            lStripBytes = LStripSize;
            syncIndex   = 0;
            frameIndex  = 0;
        }
    }

    return true;
}
